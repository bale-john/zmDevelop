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
#include "Config.h"
#include "ConstDef.h"
#include "Log.h"
#include "Process.h"
#include "Zk.h"
#include "LoadBalance.h"
#include "ServiceListener.h"
#include "x86_spinlocks.h"
using namespace std;


typedef void* (MultiThread::*us)(void*);
us myUS = &MultiThread::updateService;

MultiThread::MultiThread(Zk* zk_input) : zk(zk_input) {
	conf = Config::getInstance();
	updateServiceLock = SPINLOCK_INITIALIZER;
}

MultiThread::~MultiThread() {

}

bool MultiThread::isOnluOneUp(string node, int val) {
	bool ret = true;
	size_t pos = node.rfind('/');
	string serviceFather = node.substr(0, pos);
	spinlock_lock(&updateServiceLock);
	if ((conf->serviceFatherStatus)[serviceFather][val+1] > 1) {
		//在锁内部直接把serviceFatherStatus改变了，up的-1，down的+1；
		--((conf->serviceFatherStatus)[serviceFather][STATUS_UP+1]);
		++((conf->serviceFatherStatus)[serviceFather][STATUS_DOWN+1]);
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

bool MultiThread::isOnlyOneUp(string key, int val) {
    int ret = (conf->serviceFatherStatus)[key][STATUS_UP+1];
    return ret == 1;
}

//更新线程。原来的设计是随机的更新顺序，我觉得这是不合理的，应该使用先来先服务的类型
//这里判断是否为空需要加锁吗？感觉应该不需要吧，如果有另一个线程正在写，empty()将会返回什么值？
//这里用先来先服务会有问题，如果一个节点被重复改变两次，怎么处理？
void* MultiThread::updateService(void* args) {
	while (1) {
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
		//1.更新zk，这应该不用设置watch，那最好就用zk类来做咯
        updateZk(key, val);
		//2.更新conf
		updateConf(key, val);
		usleep(1000);
	}
    pthread_exit(0);
}

void *checkService(void* args) {
	cout << "check" << endl;
	cout << (*(int*)args) << endl;
    pthread_exit(0);
}

//TODO 主线程肯定是要考虑配置重载什么的这些事情的
int MultiThread::runMainThread() {
	int schedule = NOSCHEDULE;
    //没有考虑异常，如pthread不成功等
	pthread_create(&updateServiceThread, NULL, (void* (*)(void*))myUS, NULL);
	unordered_map<string, unordered_set<string>> ServiceFatherToIp = conf->getServiceFatherToIp();
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
		newThreadNum = ServiceFatherToIp.size();
		//线程需要开满，且需要调度.需要调度与否必须通过参数传一个flag进去
		if (newThreadNum > MAX_THREAD_NUM) {
			newThreadNum = MAX_THREAD_NUM;
			//todo 这个变量作为flag，只有主线程可以修改，但是所有的检查线程都要读它，这里是否需要加锁呢
			//todo 我应该先改变schedule的值还是先创建新线程呢？
			if (schedule == NOSCHEDULE) {
				schedule = SCHEDULE;
			}
			for (; oldThreadNum < newThreadNum; ++oldThreadNum) {
				pthread_create(&(checkServiceThread[oldThreadNum]), NULL, checkService, &schedule);
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
					pthread_create(&(checkServiceThread[oldThreadNum]), NULL, checkService, &schedule);
				}
			}
		}
		//这里为什么要sleep(2)也不是很清楚
		sleep(2);
	}
	//todo 退出标识，退出动作等等都还没写
    return 0;
}
