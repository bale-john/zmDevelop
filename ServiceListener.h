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
	static ServiceListener* slInstance;
	zhandle_t* zh;
	//key is serviceFather and value is ipPort
	unordered_map<string, unordered_set<string>> serviceFatherToIp;
	/*
	key is service father and value is the number of ipPort with different types.
	It's used for check weather there are only one service alive 
	*/
	unordered_map<string, vector<int>> serviceFatherStatus;

	Config* conf;
	LoadBalance* lb;
	//there exist some common method. I should make a base class maybe
	int initEnv();
	int destroyEnv();
    int zkGetChildren(const string path, struct String_vector* children);
    size_t getIpNum(const string& serviceFather);


public:
	static ServiceListener* getInstance();
	~ServiceListener();

	int getAllIp();

	int loadService(string path, string serviceFather, string ipPort, vector<int>& );
	int loadAllService();

    int zkGetNode(const char* path, char* data, int* dataLen);
    int addChildren(const string serviceFather, struct String_vector children);

    int getAddrByHost(const char* host, struct in_addr* addr);

    void modifyServiceFatherToIp(const string op, const string& path);
    unordered_map<string, unordered_set<string>>& getServiceFatherToIp();

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
