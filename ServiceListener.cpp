#include <string>
#include <vector>
#include <iostream>
#include <zookeeper.h>
#include <signal.h>
#include <sys/types.h>
#include <zk_adaptor.h>
#include <netdb.h>
#include "Config.h"
#include "ServiceItem.h"
#include "ServiceListener.h"
#include "LoadBalance.h"
#include "Log.h"
#include "ConstDef.h"
#include "Util.h"
#include "x86_spinlocks.h"
using namespace std;

ServiceListener* ServiceListener::slInstance = NULL;

ServiceListener* ServiceListener::getInstance() {
	if (!slInstance) {
		slInstance = new ServiceListener();
	}
	return slInstance;
}

int ServiceListener::destroyEnv() {
	if (zh) {
		LOG(LOG_INFO, "zookeeper close. func %s, line %d", __func__, __LINE__);
		zookeeper_close(zh);
		zh = NULL;
	}
    slInstance = NULL;
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
	serviceFatherToIpLock = SPINLOCK_INITIALIZER;
	serviceFatherStatusLock = SPINLOCK_INITIALIZER;
    watchFlagLock = SPINLOCK_INITIALIZER;
	conf = Config::getInstance();
	lb = LoadBalance::getInstance();
	//It makes sense. Make all locks occur in one function
	modifyServiceFatherToIp(CLEAR, "");
	serviceFatherStatus.clear();
}

ServiceListener::~ServiceListener() {
	destroyEnv();
}

//path is the path of ipPort
void ServiceListener::modifyServiceFatherToIp(const string op, const string& path) {
	if (op == CLEAR) {
		spinlock_lock(&serviceFatherToIpLock);
		serviceFatherToIp.clear();
		spinlock_unlock(&serviceFatherToIpLock);
	}
	size_t pos = path.rfind('/');
	string serviceFather = path.substr(0, pos);
	string ipPort = path.substr(pos + 1);
	size_t pos2 = ipPort.rfind(':');
	string ip = ipPort.substr(0, pos2);
	string port = ipPort.substr(pos2 + 1);
    //一个很诡异的错误，如果我把下面的代码放在这里，就会导致子进程不断死亡
    /*
    int status = STATUS_UNKNOWN;
    char data[16] = {0};
    int dataLen = 16;
    int ret = zoo_get(zh, path.c_str(), 1, data, &dataLen, NULL);
    status = atoi(data);
    */
	if (op == ADD) {
		//If this ipPort has exist, no need to do anything
		if (ipExist(serviceFather, ipPort)) {
			return;
		}
        int status = STATUS_UNKNOWN;
        char data[16] = {0};
        int dataLen = 16;
        int ret = zoo_get(zh, path.c_str(), 1, data, &dataLen, NULL);
        if (ret == ZOK) {
            LOG(LOG_INFO, "get node:%s success", __FUNCTION__, path.c_str());
        }
        else if (ret == ZNONODE) {
            LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path.c_str());
            return;
        }
        else {
            LOG(LOG_ERROR, "parameter error. zhandle is NULL");
            return;
        }
        status = atoi(data);
		//这里好像还少几个成员没有设置 connRetry和connTimeout
        ServiceItem item;
        item.setServiceFather(serviceFather);
        item.setStatus(status);
        item.setHost(ip);
        item.setPort(atoi(port.c_str()));
        struct in_addr addr;
		getAddrByHost(ip.c_str(), &addr);
		item.setAddr(&addr);

		conf->deleteService(path);
		conf->addService(path, item);
		//actually serviceFather sure should exist. no need to discuss
		/*
		if (!serviceFatherExist(serviceFather)) {
			spinlock_lock(&serviceFatherToIpLock);
			serviceFatherToIp.insert(make_pair(serviceFather, unordered_set<string> ()));
            serviceFatherToIp[serviceFather].insert(ipPort);
            spinlock_unlock(&serviceFatherToIpLock);

            modifyServiceFatherStatus(serviceFather, status, 1);
		}
		else {
            auto it = serviceFatherToIp.find(serviceFather);
            if ((it->second).find(ipPort) == (it->second).end()) {
                serviceFatherToIp[serviceFather].insert(ipPort);
                //modify the serviceFatherStatus.serviceFatherStatus changed too
                modifyServiceFatherStatus(serviceFather, status, 1);
            }
		}*/
        addIpPort(serviceFather, ipPort);
        modifyServiceFatherStatus(serviceFather, status, 1);
	}
	if (op == DELETE) {
		if (!serviceFatherExist(serviceFather)) {
			LOG(LOG_DEBUG, "service father: %s doesn't exist", serviceFather.c_str());
		}
		else if (!ipExist(serviceFather, ipPort)){
			LOG(LOG_DEBUG, "service father: %s doesn't have ipPort %s", serviceFather.c_str(), ipPort.c_str());
		}
		else {
			LOG(LOG_DEBUG, "delete service father %s, ip port %s", serviceFather.c_str(), ipPort.c_str());
			deleteIpPort(serviceFather, ipPort);
            int status = (conf->getServiceItem(path)).getStatus();
            modifyServiceFatherStatus(serviceFather, status, -1);
		}
		//uodate serviceMap
		conf->deleteService(path);
	}
#ifdef DEBUGS
	cout << op << 666666666 << path << endl;
    for (auto it1 = serviceFatherToIp.begin(); it1 != serviceFatherToIp.end(); ++it1) {
        if (it1->first != "/qconf/demo/test/hosts") {
            continue;
        }
        cout << it1->first << endl;
        for (auto it2 = (it1->second).begin(); it2 != (it1->second).end(); ++it2) {
            cout << *it2 << " ";
        }
        cout << endl;
    }
#endif
#ifdef DEBUGSS
	cout << op << 77777777 << path << endl;
	for (auto it = serviceFatherStatus.begin(); it != serviceFatherStatus.end(); ++it) {
        if (it->first != "/qconf/demo/test/hosts") {
            continue;
        }
		cout << it->first << endl;
		for (auto it1 = (it->second).begin(); it1 != (it->second).end(); ++it1) {
			cout << *it1 << " ";
		}
		cout << endl;
	}
#endif
#ifdef DEBUGSSS
	cout << op << 888888 << path << endl;
	Util::printServiceMap();
#endif
}

void ServiceListener::processDeleteEvent(zhandle_t* zhandle, const string& path) {
	//It must be a service node. Because I do zoo_get only in service node
	//update serviceFatherToIp
	ServiceListener* sl = ServiceListener::getInstance();
	sl->modifyServiceFatherToIp(DELETE, path);
}

void ServiceListener::processChildEvent(zhandle_t* zhandle, const string& path) {
	//It must be a service father node. Because I do zoo_get_children only in service father node
	ServiceListener* sl = ServiceListener::getInstance();
	struct String_vector children = {0};
	int ret = zoo_get_children(zhandle, path.c_str(), 1, &children);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get children success");
		if (children.count <= (int)sl->getIpNum(path)) {
			LOG(LOG_INFO, "actually It's a delete event");
		}
		else {
			//感觉很低效，可是新建了一个节点，好像只能把所有子节点都获取来，重新加一遍
			LOG(LOG_INFO, "add new service");
			for (int i = 0; i < children.count; ++i) {
				string ipPort = string(children.data[i]);
				sl->modifyServiceFatherToIp(ADD, path + "/" + ipPort);
			}
		}
		return;
	}
	else if (ret == ZNONODE) {
		LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path.c_str());
		//这个serviceFather不存在了，可能是删除了，这个事件由readBalance完成，也许这里也应该做点什么
		//serviceFatherToIp.erase(path);
        return;
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
		return;
	}
}

void ServiceListener::processChangedEvent(zhandle_t* zhandle, const string& path) {
	//ServiceListener* sl = ServiceListener::getInstance();
	Config* conf = Config::getInstance();
	//int oldStatus = (conf->getServiceItem(path)).getStatus();

	int newStatus = STATUS_UNKNOWN;
	char data[16] = {0};
	int dataLen = 16;
	int ret = zoo_get(zhandle, path.c_str(), 1, data, &dataLen, NULL);
    if (ret == ZOK) {
        LOG(LOG_INFO, "get node:%s success", __FUNCTION__, path.c_str());
    }
    else if (ret == ZNONODE) {
        LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path.c_str());
        return;
    }
    else {
        LOG(LOG_ERROR, "parameter error. zhandle is NULL");
        return;
    }
	newStatus = atoi(data);
	/*
    size_t pos = path.rfind('/');
    string serviceFather = path.substr(0, pos);
    if (sl->getWatchFlag()) {
        sl->clearWatchFlag();
    }
    else {
	    sl->modifyServiceFatherStatus(serviceFather, oldStatus, -1);
	    sl->modifyServiceFatherStatus(serviceFather, newStatus, 1);
    }
    */
	//update serviceMap
    conf->setServiceMap(path, newStatus);
}

void ServiceListener::watcher(zhandle_t* zhandle, int type, int state, const char* path, void* context) {
	switch (type) {
		case SESSION_EVENT_DEF:
			if (state == ZOO_EXPIRED_SESSION_STATE) {
				LOG(LOG_INFO, "[ session event ] state: state");
				LOG(LOG_INFO, "restart the main loop!");
				kill(getpid(), SIGUSR2);
			}
			else {
				//todo
				LOG(LOG_INFO, "[session event] state: %d", state);
			}
			break;
		case CREATED_EVENT_DEF:
			//nothing todo
            LOG(LOG_INFO, "zookeeper watcher [ create event ] path:%s", path);
            break;
        case DELETED_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ delete event ] path:%s", path);
            processDeleteEvent(zhandle, string(path));
            break;
        case CHILD_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ child event ] path:%s", path);
            //the number of service changed
            processChildEvent(zhandle, string(path));
            break;
        case CHANGED_EVENT_DEF:
            LOG(LOG_INFO, "zookeeper watcher [ change event ] path:%s", path);
            processChangedEvent(zhandle, string(path));
            break;
	}
}

int ServiceListener::addChildren(const string serviceFather, struct String_vector children) {
    if (children.count == 0) {
       addIpPort(serviceFather, ""); 
    }
	for (int i = 0; i < children.count; ++i) {
		string ip(children.data[i]);
		addIpPort(serviceFather, ip);
		LOG(LOG_INFO, "service father:%s, Ip:Port:%s", serviceFather.c_str(), ip.c_str());
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
int ServiceListener::getAllIp() {
	vector<string> serviceFather = lb->getMyServiceFather();
	for (auto it = serviceFather.begin(); it != serviceFather.end(); ++it) {
		struct String_vector children = {0};
		//get all ipPort belong to this serviceFather
		int status = zkGetChildren(*it, &children);
		if (status != M_OK) {
			LOG(LOG_ERROR, "get IP:Port failed. serviceFather:%s", (*it).c_str());
			deallocate_String_vector(&children);
            addIpPort(*it, "");
			continue;
		}
		//add the serviceFather and ipPort to the map serviceFatherToIp
		addChildren(*it, children);
		deallocate_String_vector(&children);
	}
#ifdef DEBUGS
    cout << 55555555555 << endl;
    for (auto it1 = serviceFatherToIp.begin(); it1 != serviceFatherToIp.end(); ++it1) {
        if (it1->first != "/qconf/demo/test/hosts") {
            continue;
        }
        cout << it1->first << endl;
        for (auto it2 = (it1->second).begin(); it2 != (it1->second).end(); ++it2) {
            cout << *it2 << " ";
        }
        cout << endl;
    }
#endif
	return 0;
}

int ServiceListener::zkGetNode(const char* path, char* data, int* dataLen) {
	if (!zh) {
		LOG(LOG_ERROR, "zhandle is NULL");
		return M_ERR;
	}
    LOG(LOG_INFO, "Path: %s, data: %s, *dataLen: %d", path, data, *dataLen);
	int ret = zoo_get(zh, path, 1, data, dataLen, NULL);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get data success");
		return M_OK;
	}
	else if (ret == ZNONODE) {
		LOG(LOG_TRACE, "%s...out...node:%s not exist.", __FUNCTION__, path);
        return M_ERR;
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
		return M_ERR;
	}
	return M_ERR;
}

int ServiceListener::getAddrByHost(const char* host, struct in_addr* addr) {
	int ret = M_ERR;
    struct hostent *ht;
    if ((ht = gethostbyname(host)) !=  NULL) {
        *addr = *((struct in_addr *)ht->h_addr);
        ret = M_OK;
    }   
    return ret;
}

//the args are repeat. But it's ok
int ServiceListener::loadService(string path, string serviceFather, string ipPort, vector<int>& st) {
	int status = STATUS_UNKNOWN;
	char data[16] = {0};
	int dataLen = 16;
	int ret = zkGetNode(path.c_str(), data, &dataLen);
	if (ret != M_OK) {
		LOG(LOG_ERROR, "get service status failed. service:%s", path.c_str());
		return M_ERR;
	}
	status = atoi(data);
    if (status < -1 || status > 2) {
        status = -1;
    }
	++(st[status + 1]);
	size_t pos = ipPort.find(':');
	string ip = ipPort.substr(0, pos);
	int port = atoi((ipPort.substr(pos+1)).c_str());
	struct in_addr addr;
	getAddrByHost(ip.c_str(), &addr);
	ServiceItem serviceItem;
	serviceItem.setStatus(status);
	serviceItem.setHost(ip);
	serviceItem.setPort(port);
	serviceItem.setAddr(&addr);
	serviceItem.setServiceFather(serviceFather);
	conf->addService(path, serviceItem);
    LOG(LOG_INFO, "load service succeed, service:%s, status:%d", path.c_str(), status);
	return M_OK;
}

int ServiceListener::loadAllService() {
	//here we need locks. Maybe we can remove it
	spinlock_lock(&serviceFatherToIpLock);
	for (auto it1 = serviceFatherToIp.begin(); it1 != serviceFatherToIp.end(); ++it1) {
		string serviceFather = it1->first;
		unordered_set<string> ips = it1->second;
		spinlock_unlock(&serviceFatherToIpLock);
		vector<int> status(4, 0);
		for (auto it2 = ips.begin(); it2 != ips.end(); ++it2) {
			string path = serviceFather + "/" + (*it2);
			loadService(path, serviceFather, *it2, status);
		}
		modifyServiceFatherStatus(serviceFather, status);
		spinlock_lock(&serviceFatherToIpLock);
	}
	spinlock_unlock(&serviceFatherToIpLock);
#ifdef DEBUGSS
	cout << 444444444 << endl;
	for (auto it = serviceFatherStatus.begin(); it != serviceFatherStatus.end(); ++it) {
        if (it->first != "/qconf/demo/test/hosts") {
            continue;
        }
		cout << it->first << endl;
		for (auto it1 = (it->second).begin(); it1 != (it->second).end(); ++it1) {
			cout << *it1 << " ";
		}
		cout << endl;
	}
#endif
#ifdef DEBUGSSS
	cout << 3333333333 << endl;
	Util::printServiceMap();
#endif
    return 0;
}

//pay attention to locks
int ServiceListener::modifyServiceFatherStatus(const string& serviceFather, int status, int op) {
	spinlock_lock(&serviceFatherStatusLock);
	serviceFatherStatus[serviceFather][status + 1] += op;
	spinlock_unlock(&serviceFatherStatusLock);
	return 0;
}

int ServiceListener::getServiceFatherStatus(const string& serviceFather, int status) {
	int ret;
	spinlock_lock(&serviceFatherStatusLock);
	ret = serviceFatherStatus[serviceFather][status + 1];
	spinlock_unlock(&serviceFatherStatusLock);
	return ret;
}

int ServiceListener::modifyServiceFatherStatus(const string& serviceFather, vector<int>& statusv) {
	spinlock_lock(&serviceFatherStatusLock);
	serviceFatherStatus[serviceFather] = statusv;
	spinlock_unlock(&serviceFatherStatusLock);
	return 0;
}

unordered_map<string, unordered_set<string>> ServiceListener::getServiceFatherToIp() {
	unordered_map<string, unordered_set<string>> ret;
	spinlock_lock(&serviceFatherToIpLock);
	ret = serviceFatherToIp;
	spinlock_unlock(&serviceFatherToIpLock);
	return ret;
}

size_t ServiceListener::getIpNum(const string& serviceFather) {
	size_t ret = 0;
	spinlock_lock(&serviceFatherToIpLock);
	if (serviceFatherToIp.find(serviceFather) != serviceFatherToIp.end()) {
        ret = serviceFatherToIp[serviceFather].size();
    }
	spinlock_unlock(&serviceFatherToIpLock);
	return ret;
}

bool ServiceListener::ipExist(const string& serviceFather, const string& ipPort) {
	bool ret = true;
	spinlock_lock(&serviceFatherToIpLock);
	if (serviceFatherToIp[serviceFather].find(ipPort) == serviceFatherToIp[serviceFather].end()) {
		ret = false;
	}
	spinlock_unlock(&serviceFatherToIpLock);
	return ret;
}

bool ServiceListener::serviceFatherExist(const string& serviceFather) {
	bool ret = true;
	spinlock_lock(&serviceFatherToIpLock);
	if (serviceFatherToIp.find(serviceFather) == serviceFatherToIp.end()) {
		ret = false;
	}
	spinlock_unlock(&serviceFatherToIpLock);
	return ret;
}

//要增加健壮性也应该在这里增加
void ServiceListener::addIpPort(const string& serviceFather, const string& ipPort) {
	spinlock_lock(&serviceFatherToIpLock);
	serviceFatherToIp[serviceFather].insert(ipPort);
	spinlock_unlock(&serviceFatherToIpLock);
}

void ServiceListener::deleteIpPort(const string& serviceFather, const string& ipPort) {
	spinlock_lock(&serviceFatherToIpLock);
	serviceFatherToIp[serviceFather].erase(ipPort);
	spinlock_unlock(&serviceFatherToIpLock);
}


//这个标记一开始是用来区分zk节点的值是由monitor去改变的还是zk自己改变的
//是为了使用serviceFatherStatus来判断是否仅剩一个up的服务节点的
//最后发现这样还是不可行，因为网络的原因等，还是无法确认每次设置了标记位之后就清楚，在设置，暂时无用
void ServiceListener::setWatchFlag() {
    spinlock_lock(&watchFlagLock);
    watchFlag = true;
    spinlock_unlock(&watchFlagLock);
}

void ServiceListener::clearWatchFlag() {
    spinlock_lock(&watchFlagLock);
    watchFlag = true;
    spinlock_unlock(&watchFlagLock);
}

bool ServiceListener::getWatchFlag(){
    bool ret;
    spinlock_lock(&watchFlagLock);
    ret = watchFlag;
    spinlock_unlock(&watchFlagLock);
    return ret;
}
