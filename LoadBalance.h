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
	unordered_map<string, string> md5ToServiceFather;
	unordered_set<string> monitor;
	unordered_set<string> ipPort;
	zhandle_t* zh;
	int initEnv();
	int destroyEnv();
	Config* conf;
public:
	LoadBalance();
	~LoadBalance();
	int getMd5ToServiceFather();
	int zkGetChildren(const string path, struct String_vector* children);
	int zkGetNode(char* md5Path, char* serviceFather, sizeof(serviceFather))
};
#endif
