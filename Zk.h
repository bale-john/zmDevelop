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
	string zkHost;
	string zkLogPath;
public:
	Zk();
	~Zk();
};
#endif