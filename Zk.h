#ifndef ZK_H
#define ZK_H
#include "ServiceItem.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
#include <zookeeper.h>
#include <zk_adaptor.h>
#include "Config.h"
using namespace std;

class Zk{
private:
	Zk();
	zhandle_t* _zh;
	int _recvTimeout;
	string _zkLogPath;
	string _zkHost;
	FILE* _zkLogFile;
	Config* conf;
	static Zk* zk;
public:
	~Zk();
	static Zk* getInstance();
	int initEnv(const string zkHost, const string zkLogPath, const int recvTimeout);
	int checkAndCreateZnode(string path);
	bool znodeExist(const string& path);
    int createZnode(string path);
    void zErrorHandler(const int& ret);
    int registerMonitor(string path);
    int setZnode(string node, string data);
    void destroyEnv();
    static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);
    static void processDeleteEvent(zhandle_t* zhandle, const string& path);
};
#endif
