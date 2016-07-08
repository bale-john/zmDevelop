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
#include "Util.h"
#include <netdb.h>
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
    //todo 这里仅仅是把这个数据结构在config里也保存一份，方面多线程类中对它的访问。因为对Config对象的访问时很简单的.
    //但是这样也有很大的缺点，就是这个数据结构在多个类里存在。感觉很浪费内存，后期好好考虑一下这个东西放在哪个类里会更好
    conf->setServiceFatherToIp(serviceFatherToIp);
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
int ServiceListener::loadService(string path, string serviceFather, string ipPort) {
	int status = STATUS_UNKNOWN;
	char data[16] = {0};
	int dataLen = 16;
	zkGetNode(path.c_str(), data, &dataLen);
	status = atoi(data);
	++(conf->serviceFatherStatus[status + 1]);
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
		for (auto it2 = (it1->second).begin(); it2 != (it1->second).end(); ++it2) {
			string path = serviceFather + "/" + (*it2);
			loadService(path, serviceFather, *it2);
		}
	}
	Util::printServiceMap();
    return 0;
}
