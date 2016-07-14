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
#include "x86_spinlocks.h"
using namespace std;

class ServiceListener {
//private:
public:
	ServiceListener();
	zhandle_t* zh;
	//其中的ip是单纯的ip，没有包含前缀路径，这个结构体可以认为是这个类的核心，不应该放一份拷贝到Config里面
	unordered_map<string, unordered_set<string>> serviceFatherToIp;
	//there exist some common method. I should make a base class maybe
	int initEnv();
	int destroyEnv();
	Config* conf;
    int zkGetChildren(const string path, struct String_vector* children);
    static ServiceListener* slInstance;
    LoadBalance* lb;
    size_t getIpNum(const string& serviceFather);
    //用来保存每个serviceFather拥有各种不同类型的节点的数目
	unordered_map<string, vector<int>> serviceFatherStatus;

public:
	static ServiceListener* getInstance();
	~ServiceListener();
	int addChildren(const string serviceFather, struct String_vector children);
	int getAllIp();
	int loadService(string path, string serviceFather, string ipPort, vector<int>& );
	int loadAllService();
    int zkGetNode(const char* path, char* data, int* dataLen);
    int getAddrByHost(const char* host, struct in_addr* addr);
    void modifyServiceFatherToIp(const string op, const string& path);
    static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);
    static void processDeleteEvent(zhandle_t* zhandle, const string& path);
    static void processChildEvent(zhandle_t* zhandle, const string& path);
    static void processChangedEvent(zhandle_t* zhandle, const string& path);
    size_t getServiceFatherNum();
    int modifyServiceFatherStatus(const string& serviceFather, int status, int op);
	int modifyServiceFatherStatus(const string& serviceFather, vector<int>& statusv);
	int getServiceFatherStatus(const string& serviceFather, int status);
};
#endif
