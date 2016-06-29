#include "Util.h"
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include "Config.h"
using namespace std;

Config* Config::_instance = NULL;
int Util::trim(string& str){
	//todo
	return 0;
}

vector<string> Util::split(const string& str, const char separator){
	vector<string> res;
	string item;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == separator) {
			res.push_back(item);
			item.clear();
		}
		else {
			item = item + str[i];
		}
	}
	return res;
}

int Util::printConfig(){
	cout << "daemonMode: " << (Config::getInstance())->_daemonMode << endl; 
	cout << "autoStart: " << (Config::getInstance())->_autoStart << endl; 
	cout << "logLevel: " << (Config::getInstance())->_logLevel << endl; 
	cout << "connRetryCount: " << (Config::getInstance())->_connRetryCount << endl; 
	cout << "scanInterval: " << (Config::getInstance())->_scanInterval << endl; 
	cout << "instanceName: " << (Config::getInstance())->_instanceName << endl; 
	cout << "zkHost: " << (Config::getInstance())->_zkHost << endl; 
	cout << "zkLogPath: " << (Config::getInstance())->_zkLogPath << endl; 
	cout << "instance: " << (Config::getInstance())->_instance << endl; 
	return 0;
}