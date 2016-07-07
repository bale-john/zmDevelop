#include <string>
#include <vector>
#include <iostream>
#include <zookeeper.h>
#include <zk_adaptor.h>
#include "Config.h"
#include "ServiceItem.h"
#include "ServiceListener.h"
#include "LoadBalance.h"
using namespace std;

static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);

void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context) {

}

int ServiceListener::destroyEnv() {
	if (zh) {
		LOG(LOG_INFO, "zookeeper close. func %s, line %d", __func__, __LINE__);
		zookeeper_close(zh);
		zh = NULL;
	}
	return M_OK;
}

ServiceListener::initEnv() {
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


ServiceListener::ServiceListener() : zh(NULL) {
	conf = Config::getInstance();
	serviceFatherToIp.clear();
	initEnv();
}

ServiceListener::~ServiceListener() {
	destroyEnv();
}

int ServiceListener::addChildren(const string serviceFather, struct String_vector children) {
	for (int i = 0; i < children.count; ++i) {
		string ip(children.data[i]);
		serviceFatherToIp[serviceFather].insert(ip);
	}
	return 0;
}

//get all ip belong to my service father
int ServiceListener::getAllIp(const set<string> serviceFather) {
	for (auto it = serviceFather.begin(); it != serviceFather.end(); ++it) {
		struct String_vector children = {0};
		LoadBalance::zkGetChildren(*it, children);
		addChildren(*it, children);
	}
	return 0;
}

int ServiceListener::loadService() {
	
}