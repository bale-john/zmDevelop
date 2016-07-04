#include "Zk.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
#include "ConstDef.h"
#include "Util.h"
#include <errno.h>
using namespace std;

Zk::Zk():_zh(NULL), _recvTimeout(3000), _zkLogPath(""), _zkHost(""), _zkLogFile(NULL) {}

Zk::initEnv(const string zkHost, const string zkLogPath, const int recvTimeout) {
	Util::trim(zkLogPath);
	if (zkLogPath.size() <= 0) {
		return M_ERR;
	}
	_zkLogFile = fopen(zkLogPath, "a");
	if (!_zkLogFile) {
		LOG(LOG_ERR, "log file open failed. path:%s. error:%s", zkLogPath.c_str(), strerror(errno));
		return M_ERR;
	}
	_zkLogPath = zkLogPath;
	//set the log file stream of zookeeper
	zoo_set_log_stream(_zkLogFile);

	Util::trim(zkHost);
	if (zkHost.size() <= 0) {
		return M_ERR;
	}
	_zkHost = zkHost;
	_zh = zookeeper_init(zk_host, watcher, _recvTimeout, NULL, NULL, 0);
	if (!_zh) {
		LOG(LOG_ERR, "zookeeper_init failed. Check whether zk_host(%s) is correct or not", zkHost.c_str());
		return M_ERR;
	}
	LOG(LOG_INFO, "zookeeper init success");
	return M_OK;
}
