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
private:
	MultiThread(Zk*);
	static bool threadError;
	static MultiThread* mlInstance;
	Config* conf;
	ServiceListener* sl;
    LoadBalance* lb;
	unordered_map<string, int> updateServiceInfo;
	list<string> priority;
	//每个检查线程的pthread_t和该检车线程在线程池中的下标的对应关系
	map<pthread_t, size_t> threadPos;
	Zk* zk;
	spinlock_t updateServiceInfoLock;

public:
	~MultiThread();
	//overload
	static MultiThread* getInstance(Zk*);
    static MultiThread* getInstance();
    
	int runMainThread();
	static void* staticUpdateService(void* args);
	static void* staticCheckService(void* args);
	void checkService();
	void updateService();
	int tryConnect(string curServiceFather);
	int isServiceExist(struct in_addr *addr, char* host, int port, int timeout, int curStatus);

	int updateConf(string node, int val);
	int updateZk(string node, int val);
	bool isOnlyOneUp(string node, int val);

	static bool isThreadError();
	static void setThreadError();
	static void clearThreadError();
};
#endif
