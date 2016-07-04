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
const string zkLogPath = "zk_log";

constexpr int minLogLevel = 0;
constexpr int maxLogLevel = 6;

//log file
const string logPath = "log/";
const string logFileNamePrefix = "qconf-monitor.log";

const string MONITOR_PROCESS_NAME = "qconf-monitor";

//pid file
const string PIDFILE = "pid";

//command file
const string CMDFILE = "tmp/cmd";


#endif
