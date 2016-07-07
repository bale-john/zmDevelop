#include "Util.h"
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include "Config.h"
#include <fstream>
using namespace std;

int Util::trim(string& str) {
	size_t left = 0;
	while (left < str.size()) {
		if (str[left] == ' ' || str[left] == '\t' || str[left] == '\n' || str[left] == '\r') {
			++left;
		}
		else {
			break;
		}
	}
	size_t right = str.size() - 1;
	while (right >= left) {
		if (str[right] == ' ' || str[right] == '\t' || str[left] == '\n' || str[left] == '\r') {
			--right;
		}
		else {
			break;
		}
	}
	str = str.substr(left, right - left + 1);
	return 0;
}

vector<string> Util::split(const string& str, const char separator){
	vector<string> res;
	string item;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == separator && !item.empty()) {
			res.push_back(item);
			item.clear();
		}
		else if (str[i] != separator){
			item = item + str[i];
		}
	}
    if (!item.empty()) {
        res.push_back(item);
    }
	return res;
}

int Util::writeToFile(const string content, const string fileName) {
	ofstream file;
	//todo what to do if failed. And maybe I can learn to use the exception in cpp
	file.open(fileName);
	file << content << endl;
	file.close();
	return 0;
}

int Util::printConfig(){
	cout << "daemonMode: " << (Config::getInstance())->isDaemonMode() << endl; 
	cout << "autoStart: " << (Config::getInstance())->isAutoStart() << endl; 
	cout << "logLevel: " << (Config::getInstance())->getLogLevel() << endl; 
	cout << "connRetryCount: " << (Config::getInstance())->getConnRetryCount() << endl; 
	cout << "scanInterval: " << (Config::getInstance())->getScanInterval() << endl; 
	cout << "instanceName: " << (Config::getInstance())->getInstanceName() << endl; 
	cout << "zkHost: " << (Config::getInstance())->getZkHost() << endl; 
	cout << "zkLogPath: " << (Config::getInstance())->getZkLogPath() << endl; 
	cout << "instance: " << (Config::getInstance())->getInstance() << endl; 
	return 0;
}

int Util::printServiceMap() {
	Config* conf = Config::getInstance();
	conf->printMap();
}