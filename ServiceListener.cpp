#include <string>
#include <vector>
#include <iostream>
#include <zookeeper.h>
#include <zk_adaptor.h>
#include "Config.h"
#include "ServiceItem.h"
#include "ServiceListener.h"
#include "LoadBalance.h"
#include "Log.h"
#include "ConstDef.h"
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

int ServiceListener::initEnv() {
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


int ServiceListener::zkGetChildren(const string path, struct String_vector* children) {
	if (!zh) {
		LOG(LOG_ERROR, "zhandle is NULL");
		return M_ERR;
	}
	int ret = zoo_get_children(zh, path.c_str(), 1, children);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get children success");
		return M_OK;
	}
	else if (ret == ZNONODE) {
		LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path.c_str());
        return M_ERR;
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
		return M_ERR;
	}
	return M_ERR;
}

//get all ip belong to my service father
int ServiceListener::getAllIp(const set<string> serviceFather) {
	for (auto it = serviceFather.begin(); it != serviceFather.end(); ++it) {
		struct String_vector children = {0};
		zkGetChildren(*it, &children);
		addChildren(*it, children);
	}
#ifdef DEBUG
    cout << 55555555555 << endl;
    cout << serviceFatherToIp.size() << endl;
    for (auto it1 = serviceFatherToIp.begin(); it1 != serviceFatherToIp.end(); ++it1) {
        cout << it1->first << endl;
        for (auto it2 = (it1->second).begin(); it2 != (it1->second).end(); ++it2) {
            cout << *it2 << " ";
        }
        cout << endl;
    }
#endif
	return 0;
}

int ServiceListener::loadService() {
	return 0;
}
