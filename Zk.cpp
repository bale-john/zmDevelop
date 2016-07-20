#include <cstring>
#include <map>
#include <cstdio>
#include <signal.h>
#include <sys/types.h>
#include <string>
#include <iostream>
#include <errno.h>
#include "ConstDef.h"
#include "Config.h"
#include "Util.h"
#include "Log.h"
#include "Zk.h"
using namespace std;

extern bool _stop;
//临时解决方案，把_zkLockBuf作为非静态全局变量，使得它对所有文件可见,这里面保存的是注册的monitor的名字
char _zkLockBuf[512] = {0};

Zk* Zk::zk = NULL;

Zk* Zk::getInstance() {
	if (!zk) {
		zk = new Zk();
	}
	return zk;
}

void Zk::processDeleteEvent(zhandle_t* zhandle, const string& path) {
	Zk* zk = Zk::getInstance();
    Config* conf = Config::getInstance();
	if (path == conf->getNodeList()) {
		LOG(LOG_INFO, "node %s is removed", path.c_str());
		zk->createZnode(path);
	}
	if (path == conf->getMonitorList()) {
		LOG(LOG_INFO, "monitor dir %s is removed. Restart mail loop", path.c_str());
        _stop = true;
	}
}

/*
The watcher in Zk will deal with events as follow:
1. zk disconnect with zookeeper server -> restart main loop
2. md5List is removed -> create it
3. monitorList is removed -> restart main loop
*/
void Zk::watcher(zhandle_t* zhandle, int type, int state, const char* node, void* context){
	dp();
	switch (type) {
		case SESSION_EVENT_DEF:
			if (state == ZOO_EXPIRED_SESSION_STATE) {
				LOG(LOG_DEBUG, "[session state: ZOO_EXPIRED_STATA: -112]");
				//todo 是否需要watchSession？
				LOG(LOG_INFO, "restart the main loop!");
				//直接设置_stop其实是一样的效果
				kill(getpid(), SIGUSR2);
			}
			else {
				LOG(LOG_DEBUG, "[ session state: %d ]", state);
			}
			break;
		case CHILD_EVENT_DEF:
            LOG(LOG_DEBUG, "[ child event ] ...");
            break;
        case CREATED_EVENT_DEF:
            LOG(LOG_DEBUG, "[ created event ]...");
            break;
        case DELETED_EVENT_DEF:
            LOG(LOG_DEBUG, "[ deleted event ] ...");
            processDeleteEvent(zhandle, string(node));
            break;
        case CHANGED_EVENT_DEF:
            LOG(LOG_DEBUG, "[ changed event ] ...");
            break;
        default:
			break;
    }
}

Zk::Zk():_zh(NULL), _recvTimeout(3000), _zkLogPath(""), _zkHost(""), _zkLogFile(NULL) {
	conf = Config::getInstance();
}

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

void Zk::destroyEnv() {
	if (_zh) {
		zookeeper_close(_zh);
	}
	zk = NULL;
}

Zk::~Zk(){
	destroyEnv();
};

void Zk::zErrorHandler(const int& ret) {
	if (ret == ZSESSIONEXPIRED ||  /*!< The session has been expired by the server */
        ret == ZSESSIONMOVED ||    /*!< session moved to another server, so operation is ignored */    
        ret == ZOPERATIONTIMEOUT ||/*!< Operation timeout */
        ret == ZINVALIDSTATE)     /*!< Invliad zhandle state */
    {   
        LOG(LOG_ERROR, "API return: %s. Reinitialize zookeeper handle.", zerror(ret));
        //todo 现在的系统健壮性不强，到时候这边应该加上对应的补救措施
    	//destoryEnv();
      	//initEnv(_zkHost, _zkLogPath, _recvTimeout);
    }
    else if (ret == ZCLOSING ||    /*!< ZooKeeper is closing */
            ret == ZCONNECTIONLOSS)  /*!< Connection to the server has been lost */
    {   
        LOG(LOG_FATAL_ERROR, "connect to zookeeper Failed!. API return : %s. Try to initialize zookeeper handle", zerror(ret));
    	//todo
    	//destoryEnv();
    	//initEnv(_zkHost, _zkLogPath, _recvTimeout);
    }  
}

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
		zErrorHandler(ret);
		return false;
	}
}

//after create znode, set the watcher with zoo_exists
int Zk::createZnode(string path) {
	vector<string> nodeList = Util::split(path, '/');
	string node("");
	//root should sure be exist
	for (auto it = nodeList.begin(); it != nodeList.end(); ++it) {
        node += "/";
		node += (*it);
		int ret = zoo_create(_zh, node.c_str(), NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
		if (ret == ZOK) {
			LOG(LOG_INFO, "Create node succeeded. node: %s", node.c_str());
			//add the watcher
			struct Stat stat;
			zoo_exists(_zh, node.c_str(), 1, &stat);
		}
		else if (ret == ZNODEEXISTS) {
			LOG(LOG_INFO, "Create node .Node exists. node: %s", node.c_str());
		}
		else {
			LOG(LOG_ERROR, "create node failed. error: %s. node: %s ", zerror(ret), node.c_str());
            //todo
            zErrorHandler(ret);
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

int Zk::registerMonitor(string path) {
	while (_zh) {
		memset(_zkLockBuf, 0, sizeof(_zkLockBuf));
		int ret = zoo_create(_zh, path.c_str(), NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 
			ZOO_EPHEMERAL | ZOO_SEQUENCE, _zkLockBuf, sizeof(_zkLockBuf));
		if (ret == ZOK) {
			LOG(LOG_INFO, "Create zookeeper node succeeded. node: %s", _zkLockBuf);
		}
		else if (ret == ZNODEEXISTS) {
			LOG(LOG_INFO, "Create zookeeper node .Node exists. node: %s", _zkLockBuf);
		}
		else {
			LOG(LOG_ERROR, "create zookeeper node failed. API return : %d. node: %s ", ret, path.c_str());
            zErrorHandler(ret);
            // wait a second
            sleep(1);
            continue;
		}
		break;
	}
	if (!_zh) {
	    LOG(LOG_TRACE, "zkLock...out...error return...");
        return  M_ERR;
    }
    return M_OK;
}

int Zk::setZnode(string node, string data) {
	//todo,现在写的非常简单，几乎没有任何异常判断
	int ver = -1;
	int status = zoo_set(_zh, node.c_str(), data.c_str(), data.length(), ver);
	if (status == ZOK) {
		return 0;
	}
	else {
		LOG(LOG_ERROR, "%s failed when zoo_set node(%s), data(%d), error:%s", \
			__FUNCTION__, node.c_str(), data.c_str(), zerror(status));
		return -1;
	}
}
