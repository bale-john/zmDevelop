#include <cstring>
#include <map>
#include <algorithm>
#include <cstdio>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <iostream>
#include <errno.h>
#include "ConstDef.h"
#include "Util.h"
#include "Log.h"
#include "LoadBalance.h"
#include "Config.h"
#include "x86_spinlocks.h"
#include "Zk.h"

using namespace std;
extern bool _stop;
extern char _zkLockBuf[512];

bool LoadBalance::reBalance = false;
LoadBalance* LoadBalance::lbInstance = NULL;

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
    lbInstance = NULL;
	return M_OK;
}

LoadBalance::LoadBalance() : zh(NULL) {
	md5ToServiceFatherLock = SPINLOCK_INITIALIZER;
	conf = Config::getInstance();
	md5ToServiceFather.clear();
	monitors.clear();
	myServiceFather.clear();
}

LoadBalance::~LoadBalance(){
	destroyEnv();
}

LoadBalance* LoadBalance::getInstance() {
	if (!lbInstance) {
		lbInstance = new LoadBalance();
	}
	return lbInstance;
}

void LoadBalance::processChildEvent(zhandle_t* zhandle, const string path) {
	string monitorsPath = Config::getInstance()->getMonitorList();
	string md5Path = Config::getInstance()->getNodeList();
	//the number of registed monitors has changed. So we need rebalance
	if (path == monitorsPath) {
		LOG(LOG_INFO, "the number of monitors has changed. Rebalance...");
		setReBalance();
	}
	//serviceFather节点减少了，需要进行负载的重新均衡吗？还是只要把对应的服务接口删除就好了。
	//或许在serviceListener里监听这个更好？可以知道哪个服务被删除了
	//但是如果大量serviceFather被删除，会导致负载不均衡，又有必要进行rebalance
	//目前先直接rebalance，如何面对serviceFather数目的变化，的确是个问题
	else if (path == md5Path) {
		LOG(LOG_DEBUG, "the number of serviceFather has changed. Rebalance...");
		setReBalance();
	}
}

void LoadBalance::processChangedEvent(zhandle_t* zhandle, const string path) {
	LoadBalance* lb = LoadBalance::getInstance();
	LOG(LOG_INFO, "the data of node %s changed.", path.c_str());
	char serviceFather[256] = {0};
	int dataLen = 256;
	LOG(LOG_INFO, "md5Path: %s, serviceFather: %s, *dataLen: %d", path.c_str(), serviceFather, dataLen);
	int ret = zoo_get(zhandle, path.c_str(), 1, serviceFather, &dataLen, NULL);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get data success");
        size_t pos = path.rfind('/');
        string md5Path = path.substr(pos + 1);
		lb->updateMd5ToServiceFather(md5Path, string(serviceFather));
	}
	else if (ret == ZNONODE) {
		LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path.c_str());
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
	}
#ifdef DEBUGL
    for (auto it = (lb->md5ToServiceFather).begin(); it != (lb->md5ToServiceFather).end(); ++it) {
        cout << it->first << " " << it->second << endl;
    }
#endif
	return;
}

void LoadBalance::watcher(zhandle_t* zhandle, int type, int state, const char* path, void* context) {
	switch (type) {
		case SESSION_EVENT_DEF:
			if (state == ZOO_EXPIRED_SESSION_STATE) {
				LOG(LOG_INFO, "[ session event ] state: ZOO_EXPIRED_SESSION_STATE");
				LOG(LOG_INFO, "restart the main loop!");
				kill(getpid(), SIGUSR2);
			}
			else {
				//todo
				LOG(LOG_INFO, "[session event] state: %d", state);
			}
			break;
		//this two events should be processed by class _zk
		case CREATED_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ create event ] path:%s", path);
            break;
        case DELETED_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ delete event ] path:%s", path);
            break;
        case CHILD_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ child event ] path:%s", path);
            //redo loadBalance
            processChildEvent(zhandle, string(path));
            break;
        case CHANGED_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ change event ] path:%s", path);
            processChangedEvent(zhandle, string(path));
            //todo 意味着md5对应的serviceFather改变了。这也太奇怪了，难道是serviceFather的名字改变了？
            break;
	}
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
		LOG(LOG_ERROR, "zoo_get_children. error: %s node:%s", zerror(ret), path.c_str());
		return M_ERR;
	}
	return M_ERR;
}

int LoadBalance::zkGetNode(const char* md5Path, char* serviceFather, int* dataLen) {
	if (!zh) {
		LOG(LOG_ERROR, "zhandle is NULL");
		return M_ERR;
	}
    LOG(LOG_INFO, "md5Path: %s, serviceFather: %s, *dataLen: %d", md5Path, serviceFather, *dataLen);
	int ret = zoo_get(zh, md5Path, 1, serviceFather, dataLen, NULL);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get data success");
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

int LoadBalance::getMd5ToServiceFather() {
	string path = conf->getNodeList();
	struct String_vector md5Node = {0};
	//get all md5 node
	int ret = zkGetChildren(path, &md5Node);
	if (ret == M_ERR) {
		return M_ERR;
	}
	for (int i = 0; i < md5Node.count; ++i) {
		char serviceFather[256] = {0};
		string md5Path = conf->getNodeList() + "/" + string(md5Node.data[i]);
        int dataLen = sizeof(serviceFather);
        //get the value of md5Node which is serviceFather
		ret = zkGetNode(md5Path.c_str(), serviceFather, &dataLen);
		if (ret == M_ERR) {
			LOG(LOG_ERROR, "get value of node:%s failed", md5Path.c_str());
			continue;
		}
		updateMd5ToServiceFather(string(md5Node.data[i]), string(serviceFather));
		LOG(LOG_INFO, "md5: %s, serviceFather: %s", md5Path.c_str(), serviceFather);
	}
	deallocate_String_vector(&md5Node);
#ifdef DEBUGL
    for (auto it = md5ToServiceFather.begin(); it != md5ToServiceFather.end(); ++it) {
        cout << it->first << " " << it->second << endl;
    }
#endif
	return M_OK;
}

int LoadBalance::getMonitors(bool flag /*=false*/) {
	string path = conf->getMonitorList();
	struct String_vector monitorNode = {0};
	int ret = zkGetChildren(path, &monitorNode);
	if (ret == M_ERR) {
		LOG(LOG_ERROR, "get monitors failes. path:%s", path.c_str());
		return M_ERR;
	}
	for (int i = 0; i < monitorNode.count; ++i) {
		string monitor = string(monitorNode.data[i]);
		monitors.insert(monitor);
	}
	LOG(LOG_INFO, "There are %d monitors, I am %s", monitors.size(), _zkLockBuf);
	deallocate_String_vector(&monitorNode);
    return M_OK;
}

int LoadBalance::balance(bool flag /*=false*/) {
	vector<string> md5Node;
	spinlock_lock(&md5ToServiceFatherLock);
	for (auto it = md5ToServiceFather.begin(); it != md5ToServiceFather.end(); ++it) {
		md5Node.push_back(it->first);
	}
	spinlock_unlock(&md5ToServiceFatherLock);
#ifdef DEBUG
	cout << "LLL11111111111" << endl;
    cout << "md5 node value:" << endl;
	for (auto it = md5Node.begin(); it != md5Node.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	vector<unsigned int> sequence;
	for (auto it = monitors.begin(); it != monitors.end(); ++it) {
		unsigned int tmp = stoi((*it).substr((*it).size() - 10));
		sequence.push_back(tmp);
	}
#ifdef DEBUG
	cout << "LLL222222222222" << endl;
    cout << "sequence number of monitors registed:" << endl;
	for (auto it = sequence.begin(); it != sequence.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	sort(sequence.begin(), sequence.end());
#ifdef DEBUG
	cout << "LLL33333333333" << endl;
    cout << "sorted sequence number of monitors registed:" << endl;
	for (auto it = sequence.begin(); it != sequence.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	string monitor = string(_zkLockBuf);
	unsigned int mySeq = stoi(monitor.substr(monitor.size() - 10));
    //It's ok to use size_t. But it may have error when it's negative
    size_t rank = 0;
	for (; rank < sequence.size(); ++rank) {
		if (sequence[rank] == mySeq) {
			break;
		}
	}
    if (rank == sequence.size()) {
        LOG(LOG_INFO, "I'm connect to zk. But the monitor registed is removed. Restart main loop");
        _stop = true;
        return M_ERR;
    }
	for (size_t i = rank; i < md5Node.size(); i += monitors.size()) {
		//maybe this lock is useless
		spinlock_lock(&md5ToServiceFatherLock);
		myServiceFather.push_back(md5ToServiceFather[md5Node[i]]);
		spinlock_unlock(&md5ToServiceFatherLock);
		LOG(LOG_INFO, "my service father:%s", myServiceFather.back().c_str());
	}
#ifdef DEBUG
	cout << "LLL44444444444" << endl;
    cout << "my service father:" << endl;
	for (auto it = myServiceFather.begin(); it != myServiceFather.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	return M_OK;
}

const vector<string> LoadBalance::getMyServiceFather() {
	return myServiceFather;
}

void LoadBalance::setReBalance() {
	reBalance = true;
}

void LoadBalance::clearReBalance() {
	reBalance = false;
}

bool LoadBalance::getReBalance() {
	return reBalance;
}

void LoadBalance::updateMd5ToServiceFather(const string& md5Path, const string& serviceFather) {
    if (serviceFather.size() <= 0) {
        return;
    }
	spinlock_lock(&md5ToServiceFatherLock);
	md5ToServiceFather[md5Path] = serviceFather;
	spinlock_unlock(&md5ToServiceFatherLock);
}
