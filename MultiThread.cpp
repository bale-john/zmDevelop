#include <pthread.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <zk_adaptor.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <signal.h>
#include <string>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <list>
#include <time.h>
#include <fstream>
#include <vector>
#include <string.h>
#include <time.h>
#include "Util.h"
#include "ServiceItem.h"
#include "Config.h"
#include "ConstDef.h"
#include "Log.h"
#include "Process.h"
#include "Zk.h"
#include "LoadBalance.h"
#include "ServiceListener.h"
#include "x86_spinlocks.h"
#include "MultiThread.h"
using namespace std;

static pthread_t updateServiceThread;
static pthread_t checkServiceThread[MAX_THREAD_NUM];
//static spinlock_t updateServiceLock;
static pthread_mutex_t updateServiceLock;

MultiThread* MultiThread::mlInstance = NULL;

bool MultiThread::threadError = false;

MultiThread* MultiThread::getInstance() {
	if (!mlInstance) {
		mlInstance = new MultiThread();
	}
	return mlInstance;
}

MultiThread::MultiThread() {
	//updateServiceLock = SPINLOCK_INITIALIZER;
	pthread_mutex_init(&updateServiceLock, NULL);
	waitingIndexLock = SPINLOCK_INITIALIZER;
	hasThreadLock = SPINLOCK_INITIALIZER;
    //threadPosLock = SPINLOCK_INITIALIZER;
    pthread_mutex_init(&threadPosLock, NULL);
    //serviceFathersLock = SPINLOCK_INITIALIZER;
	pthread_mutex_init(&serviceFathersLock, NULL);
	conf = Config::getInstance();
	zk = Zk::getInstance();
	sl = ServiceListener::getInstance();
    lb = LoadBalance::getInstance();
    serviceFathers = lb->getMyServiceFather();
	serviceFatherNum = serviceFathers.size();
    clearHasThread(serviceFatherNum);
	setWaitingIndex(MAX_THREAD_NUM);
}

MultiThread::~MultiThread() {
	mlInstance = NULL;
	clearThreadError();
}

bool MultiThread::isThreadError() {
	return threadError;
}

void MultiThread::setThreadError() {
	threadError = true;
}

void MultiThread::clearThreadError() {
	threadError = false;
}
//travelsal all the service under the service father to judge weather it's only one service up
bool MultiThread::isOnlyOneUp(string node) {
	ServiceListener* sl = ServiceListener::getInstance();
	bool ret = true;
	int alive = 0;
	size_t pos = node.rfind('/');
	string serviceFather = node.substr(0, pos);
	unordered_set<string> ips = (sl->getServiceFatherToIp())[serviceFather];
	for (auto it = ips.begin(); it != ips.end(); ++it) {
		string ipPath = serviceFather + "/" + (*it);
		int status = (conf->getServiceItem(ipPath)).getStatus();
		if (status == STATUS_UP) {
			++alive;
		}
		//cout << "isonlyone: " << ipPath << " " << status << " " << alive << endl;
		if (alive > 1) {
			ret = false;
            break;
		}
	}
    if (alive > 1) {
        ret = false;
    }
	return ret;
}
//judge with serviceFatherStatus
bool MultiThread::isOnlyOneUp(string node, int val) {
	ServiceListener* sl = ServiceListener::getInstance();
	bool ret = true;
	size_t pos = node.rfind('/');
	string serviceFather = node.substr(0, pos);
	pthread_mutex_lock(&updateServiceLock);
	if (sl->getServiceFatherStatus(serviceFather, STATUS_UP) > 1) {
		//在锁内部直接把serviceFatherStatus改变了，up的-1，down的+1；
        sl->setWatchFlag();
		sl->modifyServiceFatherStatus(serviceFather, STATUS_UP, -1);
		sl->modifyServiceFatherStatus(serviceFather, STATUS_DOWN, 1);
		pthread_mutex_unlock(&updateServiceLock);
		ret = false;
	}
	else {
		pthread_mutex_unlock(&updateServiceLock);
	}
	return ret;
}

int MultiThread::updateZk(string node, int val) {
	string status = to_string(val);
	return zk->setZnode(node, status);
}

int MultiThread::updateConf(string node, int val) {
	conf->setServiceMap(node, val);
	return 0;
}


//update service thread. comes first update first
void MultiThread::updateService() {
#ifdef DEBUGM
    cout << "in update service thread" << endl;
#endif
    LOG(LOG_INFO, "in update service thread");
	while (1) {
        if (Process::isStop() || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
		pthread_mutex_lock(&updateServiceLock);
		if (updateServiceInfo.empty()) {
			priority.clear();
			pthread_mutex_unlock(&updateServiceLock);
			usleep(1000);
			continue;
		}
		pthread_mutex_unlock(&updateServiceLock);
		string key = priority.front();
		//这需要加锁吗？如果一个线程只会操作list的头部，而另一个只会操作尾部?不确定
		pthread_mutex_lock(&updateServiceLock);
		priority.pop_front();
		pthread_mutex_unlock(&updateServiceLock);
		//是有可能在优先级队列中存在，但是在字典中不存在的，比如一个节点连续被检测到两次改变，则在优先级队列中就会出现2次，但是在map中只会有一次
		pthread_mutex_lock(&updateServiceLock);
		if (updateServiceInfo.find(key) == updateServiceInfo.end()) {
			pthread_mutex_unlock(&updateServiceLock);
			//usleep(1000);
			continue;
		}
		int val = updateServiceInfo[key];
		updateServiceInfo.erase(key);
		pthread_mutex_unlock(&updateServiceLock);
		int oldStatus = (conf->getServiceItem(key)).getStatus();
        //cout << "in update service. key: " << key << " val: " << val << endl;

		//compare the new status and old status to decide weather to update status		
		if (val == STATUS_DOWN) {
			if (oldStatus == STATUS_UP) {
				if (isOnlyOneUp(key)) {
					LOG(LOG_FATAL_ERROR, "Maybe %s is the last server that is up. \
                    But monitor CAN NOT connect to it. its Status will not change!", key.c_str());
					continue;
				}
				else {
					int res = updateZk(key, val);
					if (res != 0) {
						LOG(LOG_ERROR, "update zk failed. server %s should be %d", key.c_str(), val);
					}
					else {
						updateConf(key, val);
					}
				}
			}
			else if (oldStatus == STATUS_DOWN) {
				LOG(LOG_INFO, "service %s keeps down", key.c_str());
			}
			else if (oldStatus == STATUS_OFFLINE) {
				LOG(LOG_INFO, "service %s is off line and it can't be connected", key.c_str());
			}
			else {
				LOG(LOG_WARNING, "status: %d should not exist!", oldStatus);
			}
		}
		else if (val == STATUS_UP) {
			if (oldStatus == STATUS_DOWN) {
				int res = updateZk(key, val);
				if (res != 0) {
					LOG(LOG_ERROR, "update zk failed. server %s should be %d", key.c_str(), val);
				}
				else {
					updateConf(key, val);
				}
			}
			else if (oldStatus == STATUS_UP) {
				LOG(LOG_INFO, "service %s keeps up", key.c_str());
			}
			else if (oldStatus == STATUS_OFFLINE) {
				LOG(LOG_INFO, "service %s is off line and it can be connected", key.c_str());
			}
			else {
				LOG(LOG_WARNING, "status: %d should not exist!", oldStatus);
			}
		}
		else {
			LOG(LOG_INFO, "should not come here");
		}
		usleep(1000);
	}
#ifdef DEBUGM
    cout << "out update service" << endl;
#endif
    LOG(LOG_ERROR, "out update service");
    return;
}

int MultiThread::isServiceExist(struct in_addr *addr, char* host, int port, int timeout, int curStatus) {
    //printf("%s\n", inet_ntoa(*addr));
	bool exist = true;  
    int sock = -1, val = 1, ret = 0;
    //struct hostent *host;
    struct timeval conn_tv;
    struct timeval recv_tv;
    struct sockaddr_in serv_addr;
    fd_set readfds, writefds, errfds;
                        
    timeout = timeout <= 0 ? 1 : timeout;
                            
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(LOG_ERROR, "socket failed. error:%s", strerror(errno));
        return false;// return false is a good idea ?
    }                           

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    //serv_addr.sin_addr = *((struct in_addr *)host->h_addr);
    serv_addr.sin_addr = *addr;

    // set socket non-block
    ioctl(sock, FIONBIO, &val);

    // set connect timeout
    conn_tv.tv_sec = timeout;
    conn_tv.tv_usec = 0;

    // set recv timeout
    recv_tv.tv_sec = 1;
    recv_tv.tv_sec = 0;
    setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, &recv_tv, sizeof(recv_tv));

    // connect
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        if (errno != EINPROGRESS) {
            if (curStatus != STATUS_DOWN) {
                LOG(LOG_ERROR, "connect failed. host:%s port:%d error:%s",
                        host, port, strerror(errno));
            }
            close(sock);
            return false;
        }
    }
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    FD_ZERO(&errfds);
    FD_SET(sock, &errfds);
    ret = select(sock+1, &readfds, &writefds, &errfds, &conn_tv);
    //cout << "select ret: " << ret << endl;
    if ( ret == 0 ){
        // connect timeout
        if (curStatus != STATUS_DOWN) {
            LOG(LOG_ERROR, "connect timeout. host:%s port:%d timeout:%d error:%s",
                host, port, timeout, strerror(errno));
        }
        exist = false;
    }
    if (ret < 0) {
        if (curStatus != STATUS_DOWN) {
            LOG(LOG_ERROR, "select error. host:%s port:%d timeout:%d error:%s",
                host, port, timeout, strerror(errno));
        }
        exist = false;
    }
    else {
        if (! FD_ISSET(sock, &readfds) && ! FD_ISSET(sock, &writefds)) {
            if (curStatus != STATUS_DOWN) {
               LOG(LOG_ERROR, "select not in read fds and write fds.host:%s port:%d error:%s",
                   host, port, strerror(errno));
            }
        }
        else if (FD_ISSET(sock, &errfds)) {
            exist = false;
        }
        else if (FD_ISSET(sock, &writefds) && FD_ISSET(sock, &readfds)) {
            exist = false;
        }
        else if (FD_ISSET(sock, &readfds) || FD_ISSET(sock, &writefds)) {
            exist = true;
        }
        else {
            exist = false;
        }
    }
    close(sock);
    return exist;
}


//try to ping the ipPort to see weather it's connecteble
int MultiThread::tryConnect(string curServiceFather) {
	//这里也好浪费，我只要知道一个serviceFather，结果全都拿过来了。先写着 todo
	map<string, ServiceItem> serviceMap;
	unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
	unordered_set<string> ip = serviceFatherToIp[curServiceFather];
    int retryCount = conf->getConnRetryCount();
    time_t curTime;
    time(&curTime);
    struct tm* realTime = localtime(&curTime);
	for (auto it = ip.begin(); it != ip.end(); ++it) {
        if (Process::isStop() || LoadBalance::getReBalance() || isThreadError()) {
            break;
        } 
        //及时更新这个servicemap很重要，这样在zk那边主动操作之后，watcher将状态更新到config里，检查线程才能快速发现
        serviceMap = conf->getServiceMap();
		string ipPort = curServiceFather + "/" + (*it);
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
            cout << "ttttt5: " << ipPort << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
        /*
        some service father don't have services and we add "" to serviceFatherToIp
        so we need to judge weather It's a legal ipPort
        */
        if (serviceMap.find(ipPort) == serviceMap.end()) {
            continue;
        }
		ServiceItem item = serviceMap[ipPort];
		int oldStatus = item.getStatus();
		//If the node is STATUS_UNKNOWN or STATUS_OFFLINE, we will ignore it
		if (oldStatus == STATUS_UNKNOWN || oldStatus == STATUS_OFFLINE) {
			continue;
		}
		struct in_addr addr;
        item.getAddr(&addr);
        int curTryTimes = (oldStatus == STATUS_UP) ? 1 : 3;
		int timeout = item.getConnectTimeout() > 0 ? item.getConnectTimeout() : 3;
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
            cout << "ttttt6: " << ipPort << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
		int res = isServiceExist(&addr, (char*)item.getHost().c_str(), item.getPort(), timeout, item.getStatus());
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
            cout << "ttttt7: " << ipPort << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
        int status = (res)? 0 : 2;
        //If status is down. I will retry.
        while (curTryTimes < retryCount && status == STATUS_DOWN) {
            LOG(LOG_ERROR, "can not connect to service:%s, current try times:%d, max try times:%d", ipPort.c_str(), curTryTimes, retryCount);
            res = isServiceExist(&addr, (char*)item.getHost().c_str(), item.getPort(), timeout, item.getStatus());
            status = (res) ? 0 : 2;
            ++curTryTimes;
        }
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
            cout << "ttttt8: " << ipPort << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
#ifdef DEBUGM
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
		cout << "sssssssssssssssssssssssssssss" << endl;
		cout << "ipPort: " << ipPort << " status: " << status << " oldstatus: " << oldStatus << endl;
        }
#endif
        LOG(LOG_INFO, "|checkService| service:%s, old status:%d, new status:%d. Have tried times:%d, max try times:%d", ipPort.c_str(), oldStatus, status, curTryTimes, retryCount);
		if (status != oldStatus) {
			pthread_mutex_lock(&updateServiceLock);
            priority.push_back(ipPort);
            updateServiceInfo[ipPort] = status;
            pthread_mutex_unlock(&updateServiceLock);
		}
        if (curServiceFather == "/qconf/demo/test/hosts/host2") {
            cout << "ttttt9: " << ipPort << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
	}
    if (curServiceFather == "/qconf/demo/test/hosts/host2") {
        cout << "ttttt10: " << curServiceFather << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
    }
    return 0;
}

void MultiThread::checkService() {
	pthread_t pthreadId = pthread_self();
    pthread_mutex_lock(&threadPosLock);
	size_t pos = threadPos[pthreadId];
    pthread_mutex_unlock(&threadPosLock);
    time_t curTime;
    time(&curTime);
    struct tm* realTime = localtime(&curTime);
	while (1) {
        if (pos == 0) {
            cout << "ttttt1: " << pos << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
        if (Process::isStop() || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
        pthread_mutex_lock(&serviceFathersLock);
		string curServiceFather = serviceFathers[pos];
        pthread_mutex_unlock(&serviceFathersLock);
#ifdef DEBUGM
        if (pos == 0)
		cout << "check service thread " << pthreadId << " pos: " << pos << " current service father: " << curServiceFather << endl;
#endif
		LOG(LOG_INFO, "|checkService| pthread id %x, pthread pos %d, current service father %s", \
			(unsigned int)pthreadId, (int)pos, curServiceFather.c_str());
        if (pos == 0) {
            cout << "ttttt2: " << pos << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
		tryConnect(curServiceFather);
        if (pos == 0) {
            cout << "ttttt3: " << pos << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
        if (serviceFatherNum > MAX_THREAD_NUM) {
		    setHasThread(pos, false);
		    pos = getAndAddWaitingIndex();
		    setHasThread(pos, true);
        }
        if (pos == 0) {
            cout << "ttttt4: " << pos << " " << realTime->tm_min << " " << realTime->tm_sec << endl;
        }
        sleep(1);
	}
    return;
}

void* MultiThread::staticUpdateService(void* args) {
	MultiThread* ml = MultiThread::getInstance();
	ml->updateService();
    pthread_exit(0);
}

void* MultiThread::staticCheckService(void* args) {
	MultiThread* ml = MultiThread::getInstance();
	ml->checkService();
    pthread_exit(0);
}

int MultiThread::runMainThread() {
	//Are there any problem?
	int res = pthread_create(&updateServiceThread, NULL, staticUpdateService, NULL);
	if (res != 0) {
		setThreadError();
		LOG(LOG_ERROR, "create the update service thread error: %s", strerror(res));
	}
	int oldThreadNum = 0;
	int newThreadNum = 0;
	//If the number of service father < MAX_THREAD_NUM, one service father one thread
	//todo. Better way to reuse thread
	while (1) {
		unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
        if (Process::isStop() || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
		newThreadNum = serviceFatherToIp.size();
		//线程需要开满，且需要调度.
		if (newThreadNum > MAX_THREAD_NUM) {
			newThreadNum = MAX_THREAD_NUM;
			for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
                pthread_mutex_lock(&threadPosLock);
				res = pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, NULL);
				if (res != 0) {
					setThreadError();
					LOG(LOG_ERROR, "create the check service thread error: %s", strerror(res));
					break;
				}
				threadPos[checkServiceThread[oldThreadNum]] = oldThreadNum;
                pthread_mutex_unlock(&threadPosLock);
			}
		}
		//线程不用开满，也不需要调度
		else {
			if (newThreadNum <= oldThreadNum) {
				//some thread may be left to be idle
			}
			else {
				for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
                    pthread_mutex_lock(&threadPosLock);
					res = pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, NULL);
					if (res != 0) {
						setThreadError();
						LOG(LOG_ERROR, "create the check service thread error: %s", strerror(res));
						break;
					}
					threadPos[checkServiceThread[oldThreadNum]] = oldThreadNum;
                    pthread_mutex_unlock(&threadPosLock);
#ifdef DEBUGM
                    cout << "checkServiceThread[" << oldThreadNum << "] " << checkServiceThread[oldThreadNum] << endl;
#endif
				}
			}
		}
#ifdef DEBUGM
    //cout << "finish one round" << endl;
#endif
		sleep(2);
	}

    void* exitStatus;
    int ret = 0;
    for (int i = 0; i < oldThreadNum; ++i) {
        res = pthread_join(checkServiceThread[i], &exitStatus);
        if (res != 0) {
        	LOG(LOG_ERROR, "join check service thread error: %s", strerror(res));
        	ret = -1;
        	continue;
        }
        cout << "exit check " << i << endl;
    }
    res = pthread_join(updateServiceThread, &exitStatus);
    if (res != 0) {
    	LOG(LOG_ERROR, "join update service thread error: %s", strerror(res));
    	ret = -1;
    }
    cout << "exit update " << endl;
    cout << "fffffffff" << endl;
    clearThreadError();
    return ret;
}

void MultiThread::setWaitingIndex(int val) {
	spinlock_lock(&waitingIndexLock);
	waitingIndex = val;
	spinlock_unlock(&waitingIndexLock);
	return;
}

int MultiThread::getAndAddWaitingIndex() {
	int ret;
	spinlock_lock(&waitingIndexLock);
	ret = waitingIndex;
	waitingIndex = (waitingIndex+1) % serviceFatherNum;
	spinlock_lock(&hasThreadLock);
	while (hasThread[waitingIndex]) {
		waitingIndex = (waitingIndex+1) % serviceFatherNum;
	}
	spinlock_unlock(&hasThreadLock);
	spinlock_unlock(&waitingIndexLock);
	return ret;
}

void MultiThread::clearHasThread(int sz) {
	spinlock_lock(&hasThreadLock);
	hasThread.resize(sz, false);
	spinlock_unlock(&hasThreadLock);
	return;
}

void MultiThread::setHasThread(int index, bool val) {
	spinlock_lock(&hasThreadLock);
	hasThread[index] = val;
	spinlock_unlock(&hasThreadLock);
	return;
}

bool MultiThread::getHasThread(int index) {
	bool ret = false;
	spinlock_lock(&hasThreadLock);
	ret = hasThread[index];
	spinlock_unlock(&hasThreadLock);
	return ret;
}
