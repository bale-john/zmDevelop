#ifndef LOADBALANCE_H
#define LOADBALANCE_H
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <zookeeper.h>
#include <zk_adaptor.h>
#include "Config.h"
#include "x86_spinlocks.h"
using namespace std;

class LoadBalance {
public:
	LoadBalance();
    //use map but not unordered_map so it can be sorted autonatically
    //存的是md5节点和对应的serviceFather节点
	map<string, string> md5ToServiceFather;
	unordered_set<string> monitors;
	vector<string> myServiceFather;
	zhandle_t* zh;
	int initEnv();
	int destroyEnv();
	Config* conf;
	static LoadBalance* lbInstance;
	static bool reBalance;

	spinlock_t md5ToServiceFatherLock;
    
public:
	~LoadBalance();
	static LoadBalance* getInstance();

	int zkGetChildren(const string path, struct String_vector* children);
	int zkGetNode(const char* md5Path, char* serviceFather, int* dataLen);
	
	int getMd5ToServiceFather();
	void updateMd5ToServiceFather(const string& md5Path, const string& serviceFather);
	int getMonitors(bool flag = false);
	int balance(bool flag = false);
	const vector<string>& getMyServiceFather();

	static void watcher(zhandle_t* zhandle, int type, int state, const char* path, void* context);
	static void processChildEvent(zhandle_t* zhandle, const string path);
	static void processChangedEvent(zhandle_t* zhandle, const string path);

	static void setReBalance();
	static void clearReBalance();
	static bool getReBalance();
};
#endif
