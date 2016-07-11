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
public:
	static Config* getInstance();
	int load();
	int resetConfig();
	int isDaemonMode();
	string getMonitorHostname();
	int isAutoStart();
	int getLogLevel();
	int getConnRetryCount();
	int getScanInterval();
	string getInstanceName();
	string getZkHost();
	string getZkLogPath();
	void clearServiceMap();
	int getZkRecvTimeout();
	//todo maybe should return const string
	string getNodeList();
	string getMonitorList();
	//这个名字不应该叫add，叫set
	int addService(string ipPath, ServiceItem serviceItem);
	//todo 这个print在Util类中怎么看都应该更好，现在将就一下
	int printMap();
	map<string, ServiceItem> getServiceMap();
	int setServiceFatherToIp(unordered_map<string, unordered_set<string>> sft);
	unordered_map<string, unordered_set<string>>& getServiceFatherToIp();
	//用来保存每个serviceFather拥有各种不同类型的节点的数目
	unordered_map<string, vector<int>> serviceFatherStatus;
	int setServiceMap(string node, int val);


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
	map<string, ServiceItem> _serviceMap;
	int _zkRecvTimeout;
	int setValueInt(const string& key, const string& value);
	int setValueStr(const string& key, const string& value);
	unordered_map<string, unordered_set<string>> serviceFatherToIp;

};
#endif
