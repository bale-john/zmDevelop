#include <string>
#include <vector>
#include <iostream>
#include <zookeeper.h>
#include <signal.h>
#include <sys/types.h>
#include <zk_adaptor.h>
#include "Config.h"
#include "ServiceItem.h"
#include "ServiceListener.h"
#include "LoadBalance.h"
#include "Log.h"
#include "ConstDef.h"
#include "Util.h"
#include <netdb.h>
#include "x86_spinlocks.h"
using namespace std;

ServiceListener* ServiceListener::slInstance = NULL;

ServiceListener* ServiceListener::getInstance() {
	if (!slInstance) {
		slInstance = new ServiceListener();
	}
	return slInstance;
}
//path is the path of ipPort
//这几个函数的意义和调用关系不清晰
void ServiceListener::modifyServiceFatherToIp(const string op, const string& path) {
	if (op == CLEAR) {
		serviceFatherToIp.clear();
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
		if ((conf->getServiceMap()).find(path) == (conf->getServiceMap()).end()) {
			conf->addService(path, item);//都是需要加锁的，还没加
		}
		else {
			conf->deleteService(path);
			conf->addService(path, item);
		}

		if (serviceFatherToIp.find(serviceFather) == serviceFatherToIp.end()) {
			serviceFatherToIp.insert(make_pair(serviceFather, unordered_set<string> ()));
            serviceFatherToIp[serviceFather].insert(ipPort);
            modifyServiceFatherStatus(serviceFather, status, 1);
		}
		else {
            auto it = serviceFatherToIp.find(serviceFather);
            if ((it->second).find(ipPort) == (it->second).end()) {
                serviceFatherToIp[serviceFather].insert(ipPort);
                //modify the serviceFatherStatus.serviceFatherStatus changed too
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
                modifyServiceFatherStatus(serviceFather, status, 1);
            }
		}
	}
	if (op == DELETE) {
		if (serviceFatherToIp.find(serviceFather) == serviceFatherToIp.end()) {
			LOG(LOG_DEBUG, "service father: %s doesn't exist", serviceFather.c_str());
		}
		else if (serviceFatherToIp[serviceFather].find(ipPort) == serviceFatherToIp[serviceFather].end()){
			LOG(LOG_DEBUG, "service father: %s doesn't have ipPort %s", serviceFather.c_str(), ipPort.c_str());
		}
		else {
			LOG(LOG_DEBUG, "delete service father %s, ip port %s", serviceFather.c_str(), ipPort.c_str());
			serviceFatherToIp[serviceFather].erase(ipPort);
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
	//只可能是某个服务被删除了，因为我只去get过这个节点，后续可以加异常处理
	//update serviceFatherToIp
	ServiceListener* sl = ServiceListener::getInstance();
	sl->modifyServiceFatherToIp(DELETE, path);
}

void ServiceListener::processChildEvent(zhandle_t* zhandle, const string& path) {
	//只可能是某个serviceFather子节点变化，因为我只get_child过这个节点
	//这里这个path是serviceFather
	ServiceListener* sl = ServiceListener::getInstance();
	struct String_vector children = {0};
	int ret = zoo_get_children(zhandle, path.c_str(), 1, &children);
	if (ret == ZOK) {
		LOG(LOG_INFO, "get children success");
		if (children.count <= (int)sl->getIpNum(path)) {
			LOG(LOG_INFO, "actually It's a delete event");
		}
		else {
			//感觉很低效，可是新建了一个节点，好像只能把所有子节点都获取来，重新加一遍啊
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
		//这个serviceFather不存在了，可能是删除了 todo serviceFatherToIp这个数据结构里还是要加更多动作，比如删除serviceFather
		//serviceFatherToIp.erase(path);
        return;
	}
	else {
		LOG(LOG_ERROR, "parameter error. zhandle is NULL");
		return;
	}
}

void ServiceListener::processChangedEvent(zhandle_t* zhandle, const string& path) {
	ServiceListener* sl = ServiceListener::getInstance();
	Config* conf = Config::getInstance();
	int oldStatus = (conf->getServiceItem(path)).getStatus();

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
    size_t pos = path.rfind('/');
    string serviceFather = path.substr(0, pos);
	sl->modifyServiceFatherStatus(serviceFather, oldStatus, -1);
	sl->modifyServiceFatherStatus(serviceFather, newStatus, 1);
	//update serviceMap
	(conf->getServiceItem(serviceFather)).setStatus(newStatus);
}

void ServiceListener::watcher(zhandle_t* zhandle, int type, int state, const char* path, void* context) {
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
int ServiceListener::getAllIp() {
	vector<string> serviceFather = lb->getMyServiceFather();
	for (auto it = serviceFather.begin(); it != serviceFather.end(); ++it) {
		struct String_vector children = {0};
		//get all ipPort belong to this serviceFather
		zkGetChildren(*it, &children);
		//add the serviceFather and ipPort to the map serviceFatherToIp
		addChildren(*it, children);
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
	//todo 下面其实就是一个zoo_get，但是原来的设计中有很多错误判断，我这里先都跳过吧.而且原设计中先用了exist，原因是想知道这个节点的数据有多长，需要多大的buf去存放？
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
	//todo 关于如何加锁还没想好
    //spinlock_lock(&_getaddr_spinlock);
    struct hostent *ht; //// ht should be release?
    if ((ht = gethostbyname(host)) !=  NULL) {
        *addr = *((struct in_addr *)ht->h_addr);
        ret = M_OK;
    }   
    //spinlock_unlock(&_getaddr_spinlock);
    return ret;
}

//todo 这里参数有重复，从path就可以知道其他两个的内容了
int ServiceListener::loadService(string path, string serviceFather, string ipPort, vector<int>& st) {
	int status = STATUS_UNKNOWN;
	char data[16] = {0};
	int dataLen = 16;
	zkGetNode(path.c_str(), data, &dataLen);
	status = atoi(data);
	++(st[status + 1]);
	//todo, 这里要判断异常，比如值不是允许的那几个
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
	return 0;
}

int ServiceListener::loadAllService() {
	for (auto it1 = serviceFatherToIp.begin(); it1 != serviceFatherToIp.end(); ++it1) {
		string serviceFather = it1->first;
		vector<int> status(4, 0);
		for (auto it2 = (it1->second).begin(); it2 != (it1->second).end(); ++it2) {
			string path = serviceFather + "/" + (*it2);
			loadService(path, serviceFather, *it2, status);
		}
		//还是没有异常处理
		modifyServiceFatherStatus(serviceFather, status);
	}
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

size_t ServiceListener::getServiceFatherNum() {
	return serviceFatherToIp.size();
}

//对这种和较多类有关系的数据结构，一定要注意是否需要加锁
int ServiceListener::modifyServiceFatherStatus(const string& serviceFather, int status, int op) {
	serviceFatherStatus[serviceFather][status + 1] += op;
	return 0;
}

int ServiceListener::getServiceFatherStatus(const string& serviceFather, int status) {
	return serviceFatherStatus[serviceFather][status + 1];
}

int ServiceListener::modifyServiceFatherStatus(const string& serviceFather, vector<int>& statusv) {
	serviceFatherStatus[serviceFather] = statusv;
	return 0;
}

unordered_map<string, unordered_set<string>>& ServiceListener::getServiceFatherToIp() {
	return serviceFatherToIp;
}

size_t ServiceListener::getIpNum(const string& serviceFather) {
    if (serviceFatherToIp.find(serviceFather) == serviceFatherToIp.end()) {
        return 0;
    }
    else {
        return serviceFatherToIp[serviceFather].size();
    }
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
	conf = Config::getInstance();
	lb = LoadBalance::getInstance();
	//这是有道理的，因为后续还要加锁。把所有加锁的行为都放在modifyServiceFatherToIp里很好
	modifyServiceFatherToIp(CLEAR, "");
	serviceFatherStatus.clear();
	initEnv();
}

ServiceListener::~ServiceListener() {
	destroyEnv();
}
