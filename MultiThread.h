#ifndef MULTITHREAD_H
#define MULTITHREAD_H
#include <pthread.h>
#include <iostream>
#include <cstdio>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <list>
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

class MultiThread {
public:
//private:
	pthread_t updateServiceThread;
	pthread_t checkServiceThread[MAX_THREAD_NUM];
    void* updateService(void* args);
    void* checkService(void* args);
	Config* conf;
	unordered_map<string, int> updateServiceInfo;
	list<string> priority;
	spinlock_t updateServiceLock;
	bool isOnlyOneUp(string node, int val);
	int updateZk(string node, int val);
    int updateConf(string node, int val);
    //每个检查线程的pthread_t和该检车线程在线程池中的下标的对应关系
    map<pthread_t, size_t> threadPos;
public:
	MultiThread(Zk* , const vector<string>&);
	~MultiThread();
	int runMainThread();
	Zk* zk;
    const vector<string>& serviceFather;
    int isServiceExist(struct in_addr *addr, char* host, int port, int timeout, int curStatus);
    int tryConnect(string curServiceFather);
};
#endif
