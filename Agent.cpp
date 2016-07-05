#include <iostream>
#include <cstdio>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include "Util.h"
#include "Config.h"
#include "ConstDef.h"
#include "Log.h"
#include "Process.h"
#include "Zk.h"
using namespace std;
static Zk* _zk = NULL;
static bool _stop = false;

int main(int argc, char** argv){
	Config* conf = Config::getInstance();
	Util::printConfig();
	if (Process::isProcessRunning(MONITOR_PROCESS_NAME) == 1) {
		LOG(LOG_ERROR, "Monitor is already running.");
		return -1;
	}
	if (conf->isDaemonMode()) {
		Process::daemonize();
	}
	if (conf->isAutoStart()) {
		int childExitStatus = -1;
		int ret = Process::processKeepalive(childExitStatus, PIDFILE);
		//parent thread
		if (ret > 0) {
			return childExitStatus;
		}
		else if (ret < 0) {
			return -1;
		}
		//child process
		else {

		}
	}

    cout << "goto mainloop" << endl;
	while (1) {
        cout << "mainloop start" << endl;
		LOG(LOG_INFO, " main loop start -> !!!!!!");
		_stop = false;
		conf->clearServiceMap();
        if (!_zk) {
            dp();
            delete _zk;
        }
		_zk = new Zk();
		string zkHost = conf->getZkHost();
		string zkLogPath = conf->getZkLogPath();
		int recvTimeout = conf->getZkRecvTimeout();
		if (_zk->initEnv(zkHost, zkLogPath, recvTimeout) == M_OK) {
            cout << "111" << endl;
			LOG(LOG_INFO, "Zk init env succeeded. host:%s zk log path:%s", zkHost.c_str(), zkLogPath.c_str());
		}
		else {
            cout << "222" << endl;
			LOG(LOG_ERROR, "Zk init env failed, retry");
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}
		//check qconf_monitor_lock_node/default_instance/md5_list
		if(_zk->checkAndCreateZnode(conf->getNodeList()) == M_OK) {
			LOG(LOG_INFO, "check znode %s done", (conf->getNodeList()).c_str());
		}
		else {
			LOG(LOG_ERROR, "create znode %s failed", (conf->getNodeList()).c_str());
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}
		//check qconf_monitor_lock_node/default_instance/monitor_list
		if(_zk->checkAndCreateZnode(conf->getMonitorList()) == M_OK) {
			LOG(LOG_INFO, "check znode %s done", (conf->getMonitorList()).c_str());
		}
		else {
			LOG(LOG_ERROR, "create znode %s failed", (conf->getMonitorList()).c_str());
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}

        //seems it's important !! Remember to close it always
        zookeeper_close(_zk->_zh);
	}

    cout << "EXIT!!!" << endl;
	return 0;
}
