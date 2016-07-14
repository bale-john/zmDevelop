#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <unordered_set>
#include <map>
#include <vector>
#include <unordered_map>
#include "ServiceItem.h"
using namespace std;

class Config {
private:
	Config();
	~Config();
	static Config* _instance;
	int _daemonMode;
	string _monitorHostname;
	int _autoStart;
	int _logLevel;
	int _connRetryCount;
	int _scanInterval;
	std::string _instanceName;
	std::string _zkHost;
	std::string _zkLogPath;
	//important. the key is the full path of ipPort and the value is serviceItem of this ipPort
	map<string, ServiceItem> _serviceMap;
	int _zkRecvTimeout;

	int setValueInt(const string& key, const string& value);
	int setValueStr(const string& key, const string& value);

public:
	static Config* getInstance();
	int load();
	int resetConfig();
	int isDaemonMode();
	int isAutoStart();

	string getMonitorHostname();
	int getLogLevel();
	int getConnRetryCount();
	int getScanInterval();
	string getInstanceName();
	string getZkHost();
	string getZkLogPath();
	int getZkRecvTimeout();
	//todo maybe should return const string
	string getNodeList();
	string getMonitorList();

	map<string, ServiceItem>& getServiceMap();
	int setServiceMap(string node, int val);

	//这个名字不应该叫add，叫set，方便其他对象调用
	int addService(string ipPath, ServiceItem serviceItem);
	void deleteService(const string& ipPath);

    ServiceItem& getServiceItem(const string& ipPath);
	//put this method to class Util may be better
	int printMap();

	void clearServiceMap();
};
#endif
