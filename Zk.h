#ifndef ZK_H
#define ZK_H
#include "ServiceItem.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
#include <zookeeper.h>
#include <zk_adaptor.h>
using namespace std;
class Zk{
private:
	zhandle_t* _zh;
	int _recvTimeout;
	string _zkHost;
	string _zkLogPath;
	FILE* _zkLogFile;
public:
	Zk();
	~Zk();
	initEnv(const string zkHost, const string zkLogPath, const int recvTimeout);
};
#endif