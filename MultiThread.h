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
	//vector<pthread_t> checkServiceThread;
    void* updateService(void* args);
    //void *checkService(void* args);
	Config* conf;
	unordered_map<string, int> updateServiceInfo;
	list<string> priority;
	spinlock_t updateServiceLock;
	bool isOnluOneUp(string node, int val);
	int updateZk(string node, int val);
    int updateConf(string node, int val);
    bool isOnlyOneUp(string key, int val);
public:
	MultiThread(Zk* );
	~MultiThread();
	int runMainThread();
	Zk* zk;
};
#endif
