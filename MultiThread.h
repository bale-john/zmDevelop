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
#include <pthread.h>
#include "Util.h"
#include "Config.h"
#include "ConstDef.h"
#include "Log.h"
#include "Process.h"
#include "Zk.h"
#include "LoadBalance.h"
#include "ServiceListener.h"
using namespace std;
class MultiThread {
private:
	MultiThread();
	static bool threadError;
	static MultiThread* mlInstance;
	Config* conf;
	ServiceListener* sl;
    LoadBalance* lb;
	unordered_map<string, int> updateServiceInfo;
	list<string> priority;
	//每个检查线程的pthread_t和该检车线程在线程池中的下标的对应关系
	map<pthread_t, size_t> threadPos;
    //spinlock_t threadPosLock;
	pthread_mutex_t threadPosLock;
	Zk* zk;
	int serviceFatherNum;
	//copy of myServiceFather in loadBalance
	vector<string> serviceFathers;
    //spinlock_t serviceFathersLock;
	pthread_mutex_t serviceFathersLock;
	//标记某个service father是否有一个线程在检查它
	vector<bool> hasThread;
	//spinlock_t hasThreadLock;
	pthread_mutex_t hasThreadLock;
	//标记下一个等待被检查的service father
	int waitingIndex;
	//spinlock_t waitingIndexLock;
	pthread_mutex_t waitingIndexLock;

public:
	~MultiThread();
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
	bool isOnlyOneUp(string node);

	static bool isThreadError();
	static void setThreadError();
	static void clearThreadError();

	void setWaitingIndex(int val);
	int getAndAddWaitingIndex();

	void insertUpdateServiceInfo(const string& service, const int val);

    void clearHasThread(int sz);
    void setHasThread(int index, bool val);
    bool getHasThread(int index);
};
#endif
