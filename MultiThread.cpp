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


MultiThread::MultiThread() {
	conf = Config::getInstance();
}

MultiThread::~MultiThread() {

}

void *updateService(void* args) {
	cout << "update" << endl;
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
	pthread_create(&updateServiceThread, NULL, updateService, NULL);
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
