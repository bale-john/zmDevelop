#include "Zk.h"
#include <cstring>
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
#include "ConstDef.h"
#include "Util.h"
#include <errno.h>
#include "Log.h"
#include <errno.h>
#include "LoadBalance.h"
#include "Config.h"
using namespace std;

static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);

void watcher(){

}

LoadBalance::initEnv(){
	Config* conf = Config::getInstance();
	string zkHost = conf->getZkHost();
	int revcTimeout = conf->getZkRecvTimeout();
	zh = zookeeper_init(zkHost.c_str(), watcher, revcTimeout, NULL, NULL, 0);
	if (!zh) {
		LOG(LOG_ERROR, "zookeeper_init failed. Check whether zk_host(%s) is correct or not", zkHost.c_str());
		return M_ERR;
	}
	LOG(LOG_INFO, "zookeeper init success");
	return M_OK;
}

int destroyEnv() {
	if (zh) {
		LOG(LOG_INFO, "zookeeper close. func %s, line %d", __func__, __LINE__);
		zookeeper_close(zh);
		zh = NULL;
	}
	return M_OK;
}

LoadBalance::LoadBalance() : zh(NULL){
	initEnv();
}

LoadBalance::~LoadBalance(){
	destroyEnv();
}
