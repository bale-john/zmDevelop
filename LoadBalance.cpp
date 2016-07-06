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

void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context) {

}

int LoadBalance::initEnv(){
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

int LoadBalance::destroyEnv() {
	if (zh) {
		LOG(LOG_INFO, "zookeeper close. func %s, line %d", __func__, __LINE__);
		zookeeper_close(zh);
		zh = NULL;
	}
	return M_OK;
}

LoadBalance::LoadBalance() : zh(NULL){
	conf = Config::getInstance();
	md5ToServiceFather.clear();
	monitor.clear();
	ipPort.clear();
	initEnv();	
}

LoadBalance::~LoadBalance(){
	destroyEnv();
}

int LoadBalance::zkGetChildren(const string path, struct String_vector* children) {
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

int LoadBalance::zkGetNode(char* md5Path, char* serviceFather, sizeof(serviceFather)) {
	if (!zh) {
		LOG(LOG_ERROR, "zhandle is NULL");
		return M_ERR;
	}
	//todo 下面其实就是一个zoo_get，但是原来的设计中有很多错误判断，我这里先都跳过吧.而且原设计中先用了exist，原因是想知道这个节点的数据有多长，需要多大的buf去存放？
	int ret = zoo_get(zh, md5Path, 1, serviceFather, sizeof(serviceFather), NULL);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get serviceFather success");
		return M_OK;
	}
	else if (ret == ZNONODE) {
		LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, md5Path);
        return M_ERR;
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
		return M_ERR;
	}
	return M_ERR;
}

int LoadBalance::getMd5ToServiceFather(){
	string path = conf->getNodeList();
	struct String_vector md5Node = {0};
	int ret = zkGetChildren(path, &md5Node);
	if (ret == M_ERR) {
		return M_ERR;
	}
	for (int i = 0; i < md5Node.count; ++i) {
		char serviceFather[256] = {0};
		char md5Path[256] = {0};
		md5Path = md5Node.data[i];
		//todo 根据ret的值加入异常
		ret = zkGetNode(md5Path, serviceFather, sizeof(serviceFather));
		md5ToServiceFather[to_string(md5Path)] = to_string(serviceFather);
		LOG(LOG_INFO, "md5: %s, serviceFather: %s", to_string(md5Path), to_string(serviceFather));
	}
	return M_OK;
}