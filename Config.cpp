#include <fstream>
#include "ConstDef.h"
#include <string>
#include <vector>
#include <iostream>
#include "Config.h"
#include <unistd.h>
#include "Util.h"
#include "Log.h"
using namespace std;

Config::Config(){
	resetConfig();
}

Config::~Config(){
	delete _instance;
}

Config* Config::getInstance(){
	if (_instance == NULL){
		_instance = new Config();
		_instance->load();
		//reload the config result in the change of loglevel in Log
		Log::init(_instance->_logLevel);	
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
//    _serviceMap.clear();
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
	else if (key.substr(0, zkHost.length()) == zkHost){
		char hostname[512] = {0};
		if (gethostname(hostname, sizeof(hostname)) != 0) {
			//how to log
			//LOG("get host name failed");
			return -1;
		}
		string idc = key.substr(zkHost.length());
		vector<string> singleWord;
		singleWord = Util::split(string(hostname), '.');
		size_t i = 0;
		for (; i < singleWord.size(); ++i){
			if (singleWord[i] == idc){
				_zkHost = value;
				break;
			}
		}
		if (i == singleWord.size()){
			//LOG("idc not found");
			return -1;
		}
	}
	return 0;
}

//这里面的serviceItem是什么用的
int Config::load(){
	ifstream file;
	file.open(confPath);

	if (file.good()){
		resetConfig();
		string line;
		while (getline(file, line)) {
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
			if (key.size() == 0 || key[0] == '#') {
				continue;
			}
			//get the value
			string value = line.substr(pos + 1);
			Util::trim(value);
//			cout << key << " " << value << endl;
			setValueInt(key, value);
			setValueStr(key, value);
		}
	} 
	else {
		//todo
		cout << "config file open wrong" << endl;
	}
	file.close();
	return 0;
}

int Config::getLogLevel(){
	return _logLevel;
}
