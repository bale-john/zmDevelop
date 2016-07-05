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
public:
	zhandle_t* _zh;
	int _recvTimeout;
	string _zkLogPath;
	string _zkHost;
	FILE* _zkLogFile;
public:
	Zk();
	~Zk();
	int initEnv(const string zkHost, const string zkLogPath, const int recvTimeout);
};
#endif
