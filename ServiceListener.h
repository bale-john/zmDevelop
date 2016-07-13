#ifndef SERVICELISTENER_H
#define SERVICELISTENER_H
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
#include "ServiceItem.h"
#include "LoadBalance.h"
using namespace std;

class ServiceListener {
//private:
public:
	ServiceListener();
	zhandle_t* zh;
	//其中的ip是单纯的ip，没有包含前缀路径吧
	unordered_map<string, unordered_set<string>> serviceFatherToIp;
	//看，这些都是公共的方法，就应该提取出一个父类来的
	int initEnv();
	int destroyEnv();
	Config* conf;
    int zkGetChildren(const string path, struct String_vector* children);
    ServiceListener* slInstance;
    LoadBalance* lb;

public:
	static ServiceListener* getInstance();
	~ServiceListener();
	int addChildren(const string serviceFather, struct String_vector children);
	int getAllIp(const vector<string> serviceFather);
	int loadService(string path, string serviceFather, string ipPort, vector<int>& );
	int loadAllService();
    int zkGetNode(const char* path, char* data, int* dataLen);
    int getAddrByHost(const char* host, struct in_addr* addr);
    static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);
};
#endif
