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
    //use map but not unordered_map so it can be sorted autonatically
    //存的是md5节点和对应的serviceFather节点
	map<string, string> md5ToServiceFather;
	unordered_set<string> monitors;
	unordered_set<string> ipPort;
	set<string> myServiceFather;
	zhandle_t* zh;
	int initEnv();
	int destroyEnv();
	Config* conf;
public:
	LoadBalance();
	~LoadBalance();
	int getMd5ToServiceFather();
	static int zkGetChildren(const string path, struct String_vector* children);
	static int zkGetNode(const char* md5Path, char* serviceFather, int* dataLen);
	int getMonitors();
	int balance();
	set<string> getMyServiceFather();
};
#endif
