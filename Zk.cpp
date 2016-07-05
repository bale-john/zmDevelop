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
