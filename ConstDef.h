#ifndef CONSTDEF_H
#define CONSTDEF_H
#include <string>
using namespace std;
//config file name
const string confPath = "conf/monitor.conf"; 

//config file keys
const string daemonMode = "daemon_mode";
const string autoStart = "auto_start";
const string logLevel = "log_level";
const string connRetryCount = "connect_retry_count";
const string scanInterval = "scan_interval";
const string instanceName = "instance_name";
const string zkHost = "zookeeper.";
const string logPath = "zk_log";

constexpr int minLogLevel = 0;
constexpr int maxLogLevel = 6;

//log file
const string logPath = "log/";
const string logFileNamePrefix = "qconf-monitor.log";

#endif
