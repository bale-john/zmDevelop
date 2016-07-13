#include "Zk.h"
#include <cstring>
#include <map>
#include "x86_spinlocks.h"
#include <cstdio>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <iostream>
#include "ConstDef.h"
#include "Util.h"
#include "Log.h"
#include <errno.h>
#include "LoadBalance.h"
#include "Config.h"
#include <algorithm>
using namespace std;

extern char _zkLockBuf[512];

bool LoadBalance::reBalance = false;
spinlock_t monitorLock = SPINLOCK_INITIALIZER;
spinlock_t myServiceFatherLock = SPINLOCK_INITIALIZER;
LoadBalance* LoadBalance::lbInstance = NULL;
void LoadBalance::processChildEvent(zhandle_t* zhandle, const string path) {
	LoadBalance* lb = LoadBalance::getInstance();
	string monitorsPath = Config::getInstance()->getMonitorList();
	string md5Path = Config::getInstance()->getNodeList();
	//the number of registed monitors has changed. So we need rebalance
	if (path == monitorsPath) {
		setReBalance();
		/*
		LOG(LOG_DEBUG, "rebalance...");
		lb->setReBalance();
		lb->getMonitors(true);
		lb->balance(true);
		lb->clearReBalance();
		*/
	}
	//serviceFather节点减少了，需要进行负载的重新均衡吗？还是只要把对应的服务接口删除就好了。
	//或许在serviceListener里监听这个更好？可以知道哪个服务被删除了
	//但是如果大量serviceFather被删除，会导致负载不均衡，又有必要进行rebalance
	//目前打算放在后面做
	else if (path == md5Path) {
		LOG(LOG_DEBUG, "the number of serviceFather has changed");
	}
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
            //todo 意味着md5对应的serviceFather改变了。这也太奇怪了，但还是应该设置一个处理函数
            break;
	}
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
    lbInstance = NULL;
	return M_OK;
}

LoadBalance* LoadBalance::getInstance() {
	if (!lbInstance) {
		lbInstance = new LoadBalance();
	}
	return lbInstance;
}

LoadBalance::LoadBalance() : zh(NULL) {
	conf = Config::getInstance();
	md5ToServiceFather.clear();
	monitors.clear();
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

int LoadBalance::zkGetNode(const char* md5Path, char* serviceFather, int* dataLen) {
	if (!zh) {
		LOG(LOG_ERROR, "zhandle is NULL");
		return M_ERR;
	}
	//todo 下面其实就是一个zoo_get，但是原来的设计中有很多错误判断，我这里先都跳过吧.而且原设计中先用了exist，原因是想知道这个节点的数据有多长，需要多大的buf去存放？
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
	int ret = zkGetChildren(path, &md5Node);
	if (ret == M_ERR) {
		return M_ERR;
	}
	for (int i = 0; i < md5Node.count; ++i) {
		char serviceFather[256] = {0};
		string md5Path = conf->getNodeList() + "/" + string(md5Node.data[i]);
		//todo 根据ret的值加入异常
        int dataLen = sizeof(serviceFather);
		ret = zkGetNode(md5Path.c_str(), serviceFather, &dataLen);
		md5ToServiceFather[string(md5Node.data[i])] = string(serviceFather);
		LOG(LOG_INFO, "md5: %s, serviceFather: %s", md5Path.c_str(), serviceFather);
	}
	return M_OK;
}

int LoadBalance::getMonitors(bool flag /*=false*/) {
	string path = conf->getMonitorList();
	struct String_vector monitorNode = {0};
	int ret = zkGetChildren(path, &monitorNode);
	if (ret == M_ERR) {
		return M_ERR;
	}
	unordered_set<string> tempMonitors;
	for (int i = 0; i < monitorNode.count; ++i) {
		string monitor = string(monitorNode.data[i]);
		tempMonitors.insert(monitor);
	}
	spinlock_lock(&monitorLock);
	if (reBalance && !flag) {
		spinlock_unlock(&monitorLock);
		LOG(LOG_INFO, "It's rebalancing and I am not the rebalancer");
	}
	else {
		monitors = tempMonitors;
		spinlock_unlock(&monitorLock);
		LOG(LOG_INFO, "There are %d monitors, I am %s", monitors.size(), _zkLockBuf);
	}
    return M_OK;
}

int LoadBalance::balance(bool flag /*=false*/) {
	vector<string> md5Node;
	for (auto it = md5ToServiceFather.begin(); it != md5ToServiceFather.end(); ++it) {
		md5Node.push_back(it->first);
	}
#ifdef DEBUG
	cout << 11111111111 << endl;
    //md5节点值
    cout << "md5 node value:" << endl;
	for (auto it = md5Node.begin(); it != md5Node.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	vector<unsigned int> sequence;
	//actually we need a lock here
	for (auto it = monitors.begin(); it != monitors.end(); ++it) {
		int tmp = stoi((*it).substr((*it).size() - 10));
		sequence.push_back(tmp);
	}
#ifdef DEBUG
	cout << 222222222222 << endl;
    //monitors的序列号
    cout << "sequence number of monitors registed:" << endl;
	for (auto it = sequence.begin(); it != sequence.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	sort(sequence.begin(), sequence.end());
#ifdef DEBUG
	cout << 33333333333 << endl;
    //排序之后monitors的序列号
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
	vector<string> tempMyServiceFather;
	for (size_t i = rank; i < md5Node.size(); i += monitors.size()) {
		tempMyServiceFather.push_back(md5ToServiceFather[md5Node[i]]);
	}
	spinlock_lock(&myServiceFatherLock);
	if (reBalance && !flag) {
		spinlock_unlock(&myServiceFatherLock);
		LOG(LOG_INFO, "It's rebalancing and I am not the rebalancer");
	}
	else {
		myServiceFather = tempMyServiceFather;
		spinlock_unlock(&myServiceFatherLock);
		LOG(LOG_INFO, "There are %d monitors, I am %s", monitors.size(), _zkLockBuf);
	}
#ifdef DEBUG
	cout << 44444444444 << endl;
    //进行负载均衡后，分配到这个Monitors的serviceFather节点
    cout << "my service father:" << endl;
	for (auto it = myServiceFather.begin(); it != myServiceFather.end(); ++it) {
		cout << (*it) << endl;
	}
#endif
	return M_OK;
}

const vector<string>& LoadBalance::getMyServiceFather() {
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
