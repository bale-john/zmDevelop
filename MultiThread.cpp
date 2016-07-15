#include "MultiThread.h"
#include <pthread.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
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
using namespace std;

extern bool _stop;
static pthread_t updateServiceThread;
static pthread_t checkServiceThread[MAX_THREAD_NUM];
static spinlock_t updateServiceLock;

MultiThread* MultiThread::mlInstance = NULL;

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
	conf = Config::getInstance();
	sl = ServiceListener::getInstance();
    lb = LoadBalance::getInstance();
	updateServiceLock = SPINLOCK_INITIALIZER;
}

MultiThread::~MultiThread() {
	mlInstance = NULL;
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
		/*
		--((conf->serviceFatherStatus)[serviceFather][STATUS_UP+1]);
		++((conf->serviceFatherStatus)[serviceFather][STATUS_DOWN+1]);
		*/
		spinlock_unlock(&updateServiceLock);
		ret = false;
	}
	else {
		spinlock_unlock(&updateServiceLock);
	}
	return ret;
}

int MultiThread::updateZk(string node, int val) {
	string status = to_string(val);
	zk->setZnode(node, status);
	return 0;
}

int MultiThread::updateConf(string node, int val) {
	conf->setServiceMap(node, val);
	return 0;
}

//更新线程。原来的设计是随机的更新顺序，我觉得这是不合理的，应该使用先来先服务的类型
//这里判断是否为空需要加锁吗？感觉应该不需要吧，如果有另一个线程正在写，empty()将会返回什么值？
//这里用先来先服务会有问题，如果一个节点被重复改变两次，怎么处理？
void MultiThread::updateService() {
#ifdef DEBUGM
    cout << "in update service thread" << endl;
#endif
	while (1) {
        if (_stop || LoadBalance::getReBalance()) {
            break;
        }
		spinlock_lock(&updateServiceLock);
		if (updateServiceInfo.empty()) {
			spinlock_unlock(&updateServiceLock);
			usleep(1000);
			continue;
		}
		spinlock_unlock(&updateServiceLock);
		string key = priority.front();
		//这需要加锁吗？如果一个线程只会操作list的头部，而另一个只会操作尾部，好像不用加锁
		//spinlock_lock(&updateServiceLock);
		priority.pop_front();
		//spinlock_unlock(&updateServiceLock);
		//是有可能在优先级队列中存在，但是在字典中不存在的
		spinlock_lock(&updateServiceLock);
		if (updateServiceInfo.find(key) == updateServiceInfo.end()) {
			spinlock_unlock(&updateServiceLock);
			usleep(1000);
			continue;
		}
		int val = updateServiceInfo[key];
		updateServiceInfo.erase(key);
		spinlock_unlock(&updateServiceLock);
		//现在获取了需要更新的key和value了,要把他们的状态更新到config类里和zk上
		//如果节点死了，而且这个节点是这个serviceFather的唯一一个活着的节点，那么不改变状态，否则都要改变状态
		//我应该在Config或者loadBalance里维护一个结构，保存每个serviceFather它对应的节点的状态，这样判断是否是唯一存活的节点就方便多了！
		//我觉得这个数据结构放在LoadBalance里更合理，但是放在Config里更好做，先放在Config里吧
		//这里应该进行更多判断，原来是什么状态？
		if (val == STATUS_DOWN && isOnlyOneUp(key, val)) {
			usleep(1000);
			continue;
		}
		//可以进行更新
		//1.更新zk，这应该不用设置watch，那最好就用zk类来做
        updateZk(key, val);
		//2.更新conf
		updateConf(key, val);
		usleep(1000);
	}
#ifdef DEBUGM
    cout << "out update service" << endl;
#endif
    return;
}

int MultiThread::isServiceExist(struct in_addr *addr, char* host, int port, int timeout, int curStatus) {
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
	map<string, ServiceItem> serviceMap = conf->getServiceMap();
	unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
	unordered_set<string> ip = serviceFatherToIp[curServiceFather];
#ifdef DEBUGM
	for (auto it = ip.begin(); it != ip.end(); ++it) {
        cout << *it << endl;
		string ipPort = curServiceFather + "/" + (*it);
        cout << ipPort << endl;
		ServiceItem item = serviceMap[ipPort];
		if (item.getStatus() != STATUS_UP) {
			continue;
		}
		struct in_addr addr;
        item.getAddr(&addr);
		int timeout = item.getConnectTimeout() > 0 ? item.getConnectTimeout() : 3;
		int status = isServiceExist(&addr, (char*)item.getHost().c_str(), item.getPort(), timeout, item.getStatus());
		//todo 根据status进行分类。这里先打印出来
		cout << "sssssssssssssssssssssssssssss" << endl;
		cout << ipPort << " " << status << endl;
	}
#endif
    return 0;
}


//讲道理，这个函数只需要service father和service father和ip的对应，然后去修改updateInfo就好了,目前就最简单的，一个线程负责一个serviceFather
void MultiThread::checkService() {
    cout << "in check service thread " << endl;
	pthread_t pthreadId = pthread_self();
	size_t pos = threadPos[pthreadId];
	string curServiceFather = (lb->getMyServiceFather())[pos];
#ifdef DEBUGM
    cout << pthreadId << " " << pos << " " << serviceFather[pos] << " " << curServiceFather << endl;
#endif
	while (1) {
        if (_stop || LoadBalance::getReBalance()) {
            break;
        }
		//应该先去检查这个节点是什么状态，这里要考虑一下，如果原来就是offline肯定不用管。
		//如果原来是upline肯定需要管，如果原来是down和unknown呢？这个我觉得可能要
		//目前只检查上线的
		tryConnect(curServiceFather);
        sleep(2);
		//这里得维护一个数据结构来进行线程的调度，先放着 todo，因为目前是最简单的一个线程负责一个serviceFather
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
    //没有考虑异常，如pthread不成功等
	pthread_create(&updateServiceThread, NULL, staticUpdateService, NULL);

	//这里要考虑如何分配检查线程了，应该可以做很多文章，比如记录每个father有多少个服务，如果很多就分配两个线程等等。这里先用最简单的，线程足够的情况下，一个serviceFather一个线程
	int oldThreadNum = 0;
	int newThreadNum = 0;
	//如果serviceFather数目小于最大线程数目，每个线程一个serviceFather玩去吧
	//如果serviceFather数目大于最大线程数目，这应该是更常见的，然后就要复用线程。同样的这里怎么复用，复用哪些线程都是可以做文章的
	//先实现最简单的，谁做完了就复用谁？
	//或者新思路，重新安排ip。每个线程取一个得放，这样完全不用管线程数目的事了，所有线程还是在ip级别一视同仁怎么样？线程数目少饿仍然可能饿死。。。
	//构思了一种思路，但这种思路其实最好把serviceFather和ip等数据封装成一个类。
	//todo
	while (1) {
		unordered_map<string, unordered_set<string>> serviceFatherToIp = sl->getServiceFatherToIp();
#ifdef DEBUGM
        cout << "xxxxxxxxxxx" << endl;
#endif
        if (_stop || LoadBalance::getReBalance()) {
            break;
        }
		newThreadNum = serviceFatherToIp.size();
		//线程需要开满，且需要调度.需要调度与否必须通过参数传一个flag进去
		if (newThreadNum > MAX_THREAD_NUM) {
			newThreadNum = MAX_THREAD_NUM;
			//todo 这个变量作为flag，只有主线程可以修改，但是所有的检查线程都要读它，这里是否需要加锁呢
			//todo 我应该先改变schedule的值还是先创建新线程呢？
			if (schedule == NOSCHEDULE) {
				schedule = SCHEDULE;
			}
			for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
				pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, &schedule);
				threadPos[checkServiceThread[oldThreadNum]] = oldThreadNum;
			}
		}
		//线程不用开满，也不需要调度
		else {
			if (schedule == SCHEDULE) {
				schedule = NOSCHEDULE;
			}
			//每个serviceFather一个线程，而且旧的就已经完全够用了
			if (newThreadNum <= oldThreadNum) {
				oldThreadNum = newThreadNum;
				sleep(2);
				continue;
			}
			else {
				for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
					pthread_create(checkServiceThread + oldThreadNum, NULL, staticCheckService, &schedule);
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
		//why sleep?
		sleep(2);
	}
	//todo 退出标识，退出动作等等都还没写
    void* exitStatus;
    for (int i = 0; i < oldThreadNum; ++i) {
        pthread_join(checkServiceThread[i], &exitStatus);
        cout << "exit check " << i << endl;
    }
    pthread_join(updateServiceThread, &exitStatus);
    cout << "exit update " << endl;
    cout << "fffffffff" << endl;
    return 0;
}
