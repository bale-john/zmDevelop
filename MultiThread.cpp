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

extern bool _stop;
static pthread_t updateServiceThread;
static pthread_t checkServiceThread[MAX_THREAD_NUM];
static spinlock_t updateServiceLock;

MultiThread* MultiThread::mlInstance = NULL;

bool MultiThread::threadError = false;

MultiThread* MultiThread::getInstance(Zk* zk_input) {
	if (!mlInstance) {
		mlInstance = new MultiThread(zk_input);
	}
	return mlInstance;
}

MultiThread* MultiThread::getInstance() {
    return mlInstance;
}

MultiThread::MultiThread(Zk* zk_input) : zk(zk_input) {
	updateServiceLock = SPINLOCK_INITIALIZER;
	waitingIndexLock = SPINLOCK_INITIALIZER;
	conf = Config::getInstance();
	sl = ServiceListener::getInstance();
    lb = LoadBalance::getInstance();
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

bool MultiThread::isOnlyOneUp(string node, int val) {
	ServiceListener* sl = ServiceListener::getInstance();
	bool ret = true;
	size_t pos = node.rfind('/');
	string serviceFather = node.substr(0, pos);
	spinlock_lock(&updateServiceLock);
	if (sl->getServiceFatherStatus(serviceFather, val) > 1) {
		//在锁内部直接把serviceFatherStatus改变了，up的-1，down的+1；
		sl->modifyServiceFatherStatus(serviceFather, STATUS_UP, 1);
		sl->modifyServiceFatherStatus(serviceFather, STATUS_DOWN, -1);
		spinlock_unlock(&updateServiceLock);
		ret = false;
	}
	else {
		spinlock_unlock(&updateServiceLock);
	}
	return ret;
}
//todo 
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
	while (1) {
        if (_stop || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
		spinlock_lock(&updateServiceLock);
		if (updateServiceInfo.empty()) {
			priority.clear();
			spinlock_unlock(&updateServiceLock);
			//why sleep? I think there is no nessarity
			//usleep(1000);
			continue;
		}
		spinlock_unlock(&updateServiceLock);
		string key = priority.front();
		//这需要加锁吗？如果一个线程只会操作list的头部，而另一个只会操作尾部?不确定
		//spinlock_lock(&updateServiceLock);
		priority.pop_front();
		//spinlock_unlock(&updateServiceLock);
		//是有可能在优先级队列中存在，但是在字典中不存在的，比如一个节点连续被检测到两次改变，则在优先级队列中就会出现2次，但是在map中只会有一次
		spinlock_lock(&updateServiceLock);
		if (updateServiceInfo.find(key) == updateServiceInfo.end()) {
			spinlock_unlock(&updateServiceLock);
			//usleep(1000);
			continue;
		}
		int val = updateServiceInfo[key];
		updateServiceInfo.erase(key);
		spinlock_unlock(&updateServiceLock);
		int oldStatus = (conf->getServiceItem(key)).getStatus();
        cout << "key: " << key << " val: " << val << endl;

		//compare the new status and old status to decide weather to update status		
		if (val == STATUS_DOWN) {
			if (oldStatus == STATUS_UP) {
				if (isOnlyOneUp(key, val)) {
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
		//why sleep? 
		//usleep(1000);
	}
#ifdef DEBUGM
    cout << "out update service" << endl;
#endif
    return;
}

int MultiThread::isServiceExist(struct in_addr *addr, char* host, int port, int timeout, int curStatus) {
    printf("%s\n", inet_ntoa(*addr));
    printf("%s\n", host);
    cout << port << endl;
    cout << timeout << endl;
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
    cout << "ret: " << ret << endl;
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
            cout << "1111" << endl;
            if (curStatus != STATUS_DOWN) {
               LOG(LOG_ERROR, "select not in read fds and write fds.host:%s port:%d error:%s",
                   host, port, strerror(errno));
            }
        }
        else if (FD_ISSET(sock, &errfds)) {
            cout << "2222" << endl;
            exist = false;
        }
        else if (FD_ISSET(sock, &writefds) && FD_ISSET(sock, &readfds)) {
            cout << "3333" << endl;
            exist = false;
        }
        else if (FD_ISSET(sock, &readfds) || FD_ISSET(sock, &writefds)) {
            cout << "4444" << endl;
            exist = true;
        }
        else {
            cout << "5555" << endl;
            exist = false;
        }
    }
    close(sock);
    return exist;
}


//try to ping the ipPort to see weather it's connecteble
int MultiThread::tryConnect(string curServiceFather) {
	//这里也好浪费，我只要知道一个serviceFather，结果全都拿过来了。先写着 todo
	map<string, ServiceItem> serviceMap = conf->getServiceMap();
	unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
	unordered_set<string> ip = serviceFatherToIp[curServiceFather];
#ifdef DEBUGM
	for (auto it = ip.begin(); it != ip.end(); ++it) {
		string ipPort = curServiceFather + "/" + (*it);
		ServiceItem item = serviceMap[ipPort];
		int oldStatus = item.getStatus();
		if (oldStatus == STATUS_UNKNOWN || oldStatus == STATUS_OFFLINE) {
			continue;
		}
		struct in_addr addr;
        item.getAddr(&addr);
		int timeout = item.getConnectTimeout() > 0 ? item.getConnectTimeout() : 3;
		int res = isServiceExist(&addr, (char*)item.getHost().c_str(), item.getPort(), timeout, item.getStatus());
        int status = (res)? 0 : 2;
		//todo 根据status进行分类。这里先打印出来
		cout << "sssssssssssssssssssssssssssss" << endl;
		cout << "ipPort: " << ipPort << " status: " << status << " oldstatus: " << oldStatus << endl;
		if (status != oldStatus) {
			spinlock_lock(&updateServiceLock);
            priority.push_back(ipPort);
            updateServiceInfo[ipPort] = status;
            cout << "priority: " << priority.size() << "updateServiceInfo: " << updateServiceInfo.size() << endl;
			LOG(LOG_INFO, "|checkService| service %s new status %d", ipPort.c_str(), status);
			spinlock_unlock(&updateServiceLock);
		}
	}
#endif
    return 0;
}

void MultiThread::checkService() {
	pthread_t pthreadId = pthread_self();
	size_t pos = threadPos[pthreadId];
	string curServiceFather = (lb->getMyServiceFather())[pos];
#ifdef DEBUGM
	cout << "check service thread " << pthreadId << " pos: " << pos << " current service father: " << curServiceFather << endl;
#endif
	LOG(LOG_INFO, "|checkService| pthread id %x, pthread pos %d, current service father %s", \
		(unsigned int)pthreadId, (int)pos, curServiceFather.c_str());
	while (1) {
        if (_stop || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
		//应该先去检查这个节点是什么状态，这里要考虑一下，如果原来就是offline或者unknown就不检查
		tryConnect(curServiceFather);
        sleep(2);
		//if ()
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
//TODO 主线程肯定是要考虑配置重载什么的这些事情的
int MultiThread::runMainThread() {
	int schedule = NOSCHEDULE;
	int res = pthread_create(&updateServiceThread, NULL, staticUpdateService, NULL);
	if (res != 0) {
		setThreadError();
		LOG(LOG_ERROR, "create the update service thread error: %s", strerror(res));
	}
	//考虑如何分配检查线程，比如记录每个father有多少个服务，如果很多就分配两个线程等等。
	int oldThreadNum = 0;
	int newThreadNum = 0;
	//如果serviceFather数目小于最大线程数目，每个线程一个serviceFather
	//如果serviceFather数目大于最大线程数目，这应该是更常见的，然后就要复用线程。同样的这里怎么复用，复用哪些线程有很多方法
	//todo
	while (1) {
		unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
#ifdef DEBUGM
        cout << "xxxxxxxxxxx" << endl;
#endif
        if (_stop || LoadBalance::getReBalance() || isThreadError()) {
            break;
        }
		newThreadNum = serviceFatherToIp.size();
		//线程需要开满，且需要调度.需要调度与否通过参数传一个flag进去
		if (newThreadNum > MAX_THREAD_NUM) {
			newThreadNum = MAX_THREAD_NUM;
			setWaitingIndex(MAX_THREAD_NUM);
			//todo 这个变量作为flag，只有主线程可以修改，但是所有的检查线程都要读它，这里是否需要加锁呢
			//todo 我应该先改变schedule的值还是先创建新线程呢？
			if (schedule == NOSCHEDULE) {
				schedule = SCHEDULE;
			}
			for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
				res = pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, &schedule);
				if (res != 0) {
					setThreadError();
					LOG(LOG_ERROR, "create the check service thread error: %s", strerror(res));
					break;
				}
				threadPos[checkServiceThread[oldThreadNum]] = oldThreadNum;
			}
		}
		//线程不用开满，也不需要调度
		else {
			if (schedule == SCHEDULE) {
				schedule = NOSCHEDULE;
			}
			if (newThreadNum <= oldThreadNum) {
				//some thread may be left to be idle
			}
			else {
				for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
					res = pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, &schedule);
					if (res != 0) {
						setThreadError();
						LOG(LOG_ERROR, "create the check service thread error: %s", strerror(res));
						break;
					}
					threadPos[checkServiceThread[oldThreadNum]] = oldThreadNum;
#ifdef DEBUGM
                    cout << "checkServiceThread[" << oldThreadNum << "] " << checkServiceThread[oldThreadNum] << endl;
#endif
				}
			}
		}
#ifdef DEBUGM
    cout << "finish one round" << endl;
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
	++waitingIndex;
	spinlock_unlock(&waitingIndexLock);
	return ret;
}
