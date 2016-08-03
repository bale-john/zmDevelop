#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include "Config.h"
#include "Util.h"
#include "Log.h"
#include "ConstDef.h"
using namespace std;

Config* Config::_instance = NULL;

Config::Config(){
	pthread_mutex_init(&serviceMapLock, NULL);
	//serviceMapLock = SPINLOCK_INITIALIZER;
	resetConfig();
}

Config::~Config(){
	delete _instance;
}

Config* Config::getInstance(){
	if (!_instance){
		_instance = new Config();
		_instance->load();
		//reload the config result in the change of loglevel in Log
		Log::init(_instance->getLogLevel());	
	}
	return _instance;
}

int Config::resetConfig(){
	_autoStart = 1;
    _daemonMode = 0;
    _logLevel = 2;
    _zkHost = "127.0.0.1:2181";
    _zkLogPath = ""; 
    _monitorHostname = ""; 
    _connRetryCount = 3;
    _scanInterval = 3;
    _serviceMap.clear();
    _zkRecvTimeout = 3000;
	return 0;
}

int Config::setValueInt(const string& key, const string& value){
	int intValue = atoi(value.c_str());
	if (key == daemonMode) {
		if (intValue != 0) {
			_daemonMode = 1;
		}
	}
	else if (key == autoStart){
		if (intValue == 0) {
			_autoStart = 0;
		}
	}
	else if (key == logLevel){
		if (intValue >= minLogLevel && intValue <= maxLogLevel) {
			_logLevel = intValue;
		}
	}
	else if (key == connRetryCount){
		if (intValue > 0) {
			_connRetryCount = intValue;
		}
	}
	else if (key == scanInterval){
		if (intValue > 0) {
			_scanInterval = intValue;
		}
	}
	return 0;
}

int Config::setValueStr(const string& key, const string& value){
	if (key == instanceName){
		_instanceName = value;
	}
	else if (key == zkLogPath){
		_zkLogPath = value;
	}
	//find the zk host this monitor should focus on. Their idc should be the same
	else if (key.substr(0, zkHost.length()) == zkHost){
		char hostname[128] = {0};
		if (gethostname(hostname, sizeof(hostname)) != 0) {
			LOG(LOG_ERROR, "get host name failed");
			return -1;
		}
		string idc = key.substr(zkHost.length());
		vector<string> singleWord = Util::split(string(hostname), '.');
		size_t i = 0;
		for (; i < singleWord.size(); ++i){
			if (singleWord[i] == idc){
				_zkHost = value;
				break;
			}
		}
	}
	return 0;
}

int Config::load(){
	ifstream file;
	file.open(confPath);

	if (file.good()){
		resetConfig();
		string line;
		while (!file.eof()) {
			getline(file, line);
			Util::trim(line);
			if (line.size() <= 0 || line[0] == '#') {
				continue;
			}
			size_t pos = line.find('=');
			if (pos == string::npos){
				continue;
			}
			//get the key
			string key = line.substr(0, pos);
			Util::trim(key);
			if (key.size() == 0) {
				continue;
			}
			//get the value
			string value = line.substr(pos + 1);
			Util::trim(value);
			if (value.size() == 0) {
				continue;
			}
			setValueInt(key, value);
			setValueStr(key, value);
		}
	} 
	else {
		LOG(LOG_FATAL_ERROR, "Load configure file failed. path: %s", confPath.c_str());
		exit(-1);
	}
	file.close();
	return 0;
}

int Config::getLogLevel(){
	return _logLevel;
}

int Config::isDaemonMode(){
	return _daemonMode;
}

string Config::getMonitorHostname(){
	return _monitorHostname;
}

int Config::isAutoStart(){
	return _autoStart;
}

int Config::getConnRetryCount(){
	return _connRetryCount;
}

int Config::getScanInterval(){
	return _scanInterval;
}

string Config::getInstanceName(){
	return _instanceName;
}

string Config::getZkHost(){
	return _zkHost;
}

string Config::getZkLogPath(){
	return _zkLogPath;
}

int Config::getZkRecvTimeout() {
	return _zkRecvTimeout;
}

string Config::getNodeList() {
	//should judge weather instanceName is empty
	if (_instanceName.empty()) {
		LOG(LOG_ERROR, "instance name is empty. using default_instance");
		return LOCK_ROOT_DIR + SLASH + "default_instance" + SLASH + NODE_LIST;
	}
	return LOCK_ROOT_DIR + SLASH + _instanceName + SLASH + NODE_LIST;
}

string Config::getMonitorList() {
	if (_instanceName.empty()) {
		LOG(LOG_ERROR, "instance name is empty. using default_instance");
		return LOCK_ROOT_DIR + SLASH + "default_instance" + SLASH + MONITOR_LIST;
	}
	return LOCK_ROOT_DIR + SLASH + _instanceName + SLASH + MONITOR_LIST;
}

int Config::printMap() {
	for (auto it = _serviceMap.begin(); it != _serviceMap.end(); ++it) {
#ifdef DEBUGSSS
		if ((it->second).getServiceFather() != "/qconf/demo/test/hosts") {
			continue;
		}
#endif
		cout << it->first << endl;
		cout << "host: " << (it->second).getHost() << endl;
		cout << "port: " << (it->second).getPort() << endl;
		cout << "service father: " << (it->second).getServiceFather() << endl;
		cout << "status: " << (it->second).getStatus() << endl;
	}
    return 0;
}


int Config::addService(string ipPath, ServiceItem serviceItem) {
	pthread_mutex_lock(&serviceMapLock);
    _serviceMap[ipPath] = serviceItem;
    pthread_mutex_unlock(&serviceMapLock);
    return 0;
}

void Config::deleteService(const string& ipPath) {
	pthread_mutex_lock(&serviceMapLock);
	_serviceMap.erase(ipPath);
	pthread_mutex_unlock(&serviceMapLock);
}

map<string, ServiceItem> Config::getServiceMap() {
	map<string, ServiceItem> ret;
	pthread_mutex_lock(&serviceMapLock);
	ret = _serviceMap;
	pthread_mutex_unlock(&serviceMapLock);
	return ret;
}

int Config::setServiceMap(string node, int val) {
	pthread_mutex_lock(&serviceMapLock);
	_serviceMap[node].setStatus(val);
	pthread_mutex_unlock(&serviceMapLock);
	return 0;
}

//no necessity to add lock
void Config::clearServiceMap() {
	_serviceMap.clear();
}

ServiceItem Config::getServiceItem(const string& ipPath) {
	ServiceItem ret;
	pthread_mutex_lock(&serviceMapLock);
	ret = _serviceMap[ipPath];
	pthread_mutex_unlock(&serviceMapLock);
    return ret;
}
