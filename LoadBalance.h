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
using namespace std;

class LoadBalance {
public:
	LoadBalance();
    //use map but not unordered_map so it can be sorted autonatically
    //存的是md5节点和对应的serviceFather节点
	map<string, string> md5ToServiceFather;
	unordered_set<string> monitors;
	unordered_set<string> ipPort;
	vector<string> myServiceFather;
	zhandle_t* zh;
	int initEnv();
	int destroyEnv();
	Config* conf;
	static LoadBalance* lbInstance;
	bool reBalance;
    
public:
	~LoadBalance();
	static LoadBalance* getInstance();

	int zkGetChildren(const string path, struct String_vector* children);
	int zkGetNode(const char* md5Path, char* serviceFather, int* dataLen);
	
	int getMd5ToServiceFather();
	int getMonitors();
	int balance();
	const vector<string>& getMyServiceFather();

	static void watcher(zhandle_t* zhandle, int type, int state, const char* path, void* context);
	static void processChildEvent(zhandle_t* zhandle, const string path);

	void setReBalance();
	void clearReBalance();
};
#endif
