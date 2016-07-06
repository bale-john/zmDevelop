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
using namespace std;
class LoadBalance {
public:
	unordered_set<string> serviceFather;
	unordered_set<string> monitor;
	unordered_set<string> ipPort;
	zhandle_t* zh;
	int initEnv();
	int destroyEnv();
public:
	LoadBalance();
	~LoadBalance();
};
#endif
