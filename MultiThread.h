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
private:
	pthread_t updateServiceThread;
	pthread_t checkServiceThread[MAX_THREAD_NUM];
	//vector<pthread_t> checkServiceThread;
    //void *updateService(void* args);
    //void *checkService(void* args);
	Config* conf;
public:
	MultiThread();
	~MultiThread();
	int runMainThread();
};
#endif
