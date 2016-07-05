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
using namespace std;

static void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context);
void watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context){}
Zk::Zk():_zh(NULL), _recvTimeout(3000), _zkLogPath(""), _zkHost(""), _zkLogFile(NULL) {}

int Zk::initEnv(const string zkHost, const string zkLogPath, const int recvTimeout) {
	if (zkLogPath.size() <= 0) {
		return M_ERR;
	}
	_zkLogFile = fopen(zkLogPath.c_str(), "a");
	if (!_zkLogFile) {
		LOG(LOG_ERROR, "log file open failed. path:%s. error:%s", zkLogPath.c_str(), strerror(errno));
		return M_ERR;
	}
	_zkLogPath = zkLogPath;
	//set the log file stream of zookeeper
	zoo_set_log_stream(_zkLogFile);

	if (zkHost.size() <= 0) {
		return M_ERR;
	}
	_zkHost = zkHost;
	_zh = zookeeper_init(zkHost.c_str(), watcher, _recvTimeout, NULL, NULL, 0);
	if (!_zh) {
		LOG(LOG_ERROR, "zookeeper_init failed. Check whether zk_host(%s) is correct or not", zkHost.c_str());
		return M_ERR;
	}
	LOG(LOG_INFO, "zookeeper init success");
	return M_OK;
}

Zk::~Zk(){
};

bool Zk::znodeExist(const string& path) {
	if (!_zh) {
		return false;
	}
	struct Stat stat;
	int ret = zoo_exists(_zh, path.c_str(), 0, &stat);
	if (ret == ZOK) {
		LOG(LOG_INFO, "node exist. node: %s", path.c_str());
		return true;
	}
	else if (ret == ZNONODE) {
		LOG(LOG_INFO, "node not exist. node: %s", path.c_str());
		return false;
	}
	else {
		LOG(LOG_ERROR, "zoo_exist failed. error: %s. node: %s", zerror(ret), path.c_str());
		//todo
		//zErrorHandler(ret);
		return false;
	}
}

int Zk::createZnode(string path) {
	vector<string> nodeList = Util::split(path, '/');
	string node("");
	//根节点在zk中应该是必然存在的吧
	for (auto it = nodeList.begin(); it != nodeList.end(); ++it) {
        node += "/";
		node += (*it);
		int ret = zoo_create(_zh, node.c_str(), NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
		if (ret == ZOK) {
			LOG(LOG_INFO, "Create node succeeded. node: %s", node.c_str());
		}
		else if (ret == ZNODEEXISTS) {
			LOG(LOG_INFO, "Create node .Node exists. node: %s", node.c_str());
		}
		else {
			LOG(LOG_ERROR, "create node failed. error: %s. node: %s ", zerror(ret), node.c_str());
            //todo
            //_errHandle(r);
            return M_ERR;
		}
	}
	return M_OK;
}

int Zk::checkAndCreateZnode(string path) {
	if (path.size() <= 0) {
		return M_ERR;
	}
	if (path[0] != '/') {
		path = '/' + path;
	}
	if (path.back() == '/') {
		path.pop_back();
	}
	// check weather the node exist
	if (znodeExist(path)) {
		return M_OK;
	}
	else {
		return createZnode(path);
	}
}
