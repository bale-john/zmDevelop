#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <cstdio>
#include "Config.h"
#include "Log.h"
#include "Util.h"
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

int Util::writePid(const char* fileName) {
	int fd;
	if ((fd = open(fileName, O_WRONLY|O_TRUNC|O_CREAT, 0600)) == -1) {
		LOG(LOG_ERROR, "open pid file failed. %s", fileName);
		return -1;
	}
    char pidBuf[64] = {0};
    snprintf(pidBuf, sizeof(pidBuf), "%d", getpid());
	if (write(fd, pidBuf, sizeof(pidBuf)) < 0) {
		LOG(LOG_ERROR, "write pid file failed. %s", fileName);
		return -1;
	}
	close (fd);
	return 0;
}

int Util::writeToFile(const string content, const string fileName) {
	ofstream file;
	//todo what to do if failed.
	file.open(fileName);
	file << content << endl;
	file.close();
	return 0;
}

int Util::printConfig(){
	LOG(LOG_INFO, "daemonMode: %d", Config::getInstance()->isDaemonMode());
	LOG(LOG_INFO, "autoStart: %d", Config::getInstance()->isAutoStart());
	LOG(LOG_INFO, "logLevel: %d", Config::getInstance()->getLogLevel());
	LOG(LOG_INFO, "connRetryCount: %d", Config::getInstance()->getConnRetryCount());
	LOG(LOG_INFO, "scanInterval: %d", Config::getInstance()->getScanInterval());
	LOG(LOG_INFO, "instanceName: %s", (Config::getInstance()->getInstanceName()).c_str());
	LOG(LOG_INFO, "zkHost: %s", (Config::getInstance()->getZkHost()).c_str());
	LOG(LOG_INFO, "zkLogPath: %s", (Config::getInstance()->getZkLogPath()).c_str());
	return 0;
}

int Util::printServiceMap() {
	Config* conf = Config::getInstance();
	conf->printMap();
    return 0;
}
