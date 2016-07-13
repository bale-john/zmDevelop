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

//process name
const string MONITOR_PROCESS_NAME = "qconf-monitor";

//pid file
const string PIDFILE = "pid";

//command file
const string CMDFILE = "tmp/cmd";

//return status
constexpr int M_OK = 0;
constexpr int M_ERR = -1;

// server status define
#define STATUS_UNKNOWN  -1
#define STATUS_UP        0
#define STATUS_OFFLINE   1
#define STATUS_DOWN      2

//znode and path
const string LOCK_ROOT_DIR = "/qconf_monitor_lock_node";
const string DEFAULT_INSTANCE_NAME = "default_instance";
const string MONITOR_LIST = "monitor_list";
const string SLASH = "/";
// I think it's better to call it MD5_LIST
const string NODE_LIST = "md5_list";

//multi thread
#ifdef DEBUG_FLAGS
constexpr int MAX_THREAD_NUM = 5;
#else
constexpr int MAX_THREAD_NUM = 128;
#endif
constexpr int SCHEDULE = 1;
constexpr int NOSCHEDULE = 0;

//op of mpdify serviceFatherToIp
const string DELETE = "delete";
const string ADD = "add";
const string CLEAR = "clear"

#endif
