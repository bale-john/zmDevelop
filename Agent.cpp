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
#include "LoadBalance.h"
#include "ServiceListener.h"
#include "x86_spinlocks.h"
#include "MultiThread.h"
using namespace std;
static Zk* _zk = NULL;
//if _zk disconnect with server. _stop will be true and main loop will be reiterate
extern bool _stop;

int main(int argc, char** argv){
	Config* conf = Config::getInstance();
	Util::printConfig();
#ifdef REALSE
	if (Process::isProcessRunning(MONITOR_PROCESS_NAME) == 1) {
		LOG(LOG_ERROR, "Monitor is already running.");
		return -1;
	}
#endif
	if (conf->isDaemonMode()) {
		Process::daemonize();
	}
	if (conf->isAutoStart()) {
		int childExitStatus = -1;
		int ret = Process::processKeepalive(childExitStatus, PIDFILE);
		//parent process
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
    //maybe we need it to make the child process wait a while
    //sleep(2);
	while (1) {
        cout << "mainloop start" << endl;
		LOG(LOG_INFO, " main loop start -> !!!!!!");
		_stop = false;
		conf->clearServiceMap();
        if (!_zk) {
            dp();
            delete _zk;
        }
		_zk = Zk::getInstance();
		string zkHost = conf->getZkHost();
		string zkLogPath = conf->getZkLogPath();
		int recvTimeout = conf->getZkRecvTimeout();

		// init zookeeper handler
		if (_zk->initEnv(zkHost, zkLogPath, recvTimeout) == M_OK) {
			LOG(LOG_INFO, "Zk init env succeeded. host:%s zk log path:%s", zkHost.c_str(), zkLogPath.c_str());
		}
		else {
			LOG(LOG_ERROR, "Zk init env failed, retry");
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}

		//check qconf_monitor_lock_node/default_instance/md5_list
		if(_zk->checkAndCreateZnode(conf->getNodeList()) == M_OK) {
			LOG(LOG_INFO, "check znode %s done. node exist", (conf->getNodeList()).c_str());
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
			LOG(LOG_INFO, "check znode %s done. node exist", (conf->getMonitorList()).c_str());
		}
		else {
			LOG(LOG_ERROR, "create znode %s failed", (conf->getMonitorList()).c_str());
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}

		// monitor register, this function should in LoadBalance
		if (_zk->registerMonitor(conf->getMonitorList() + "/monitor_") == M_OK) {
			LOG(LOG_INFO, "Monitor register success");
			//wait other monitor to register
			sleep(3);
		}
		else {
			LOG(LOG_ERROR, "Monitor register failed");
			if (_zk) {
				delete _zk;
			}
			sleep(2);
			continue;
		}
		/*
		this loop is for load balance.
		If rebalance is needed, the loop will be reiterate
		*/
		while (1) {
			LOG(LOG_INFO, " second loop start -> !!!!!!");
			LoadBalance::clearReBalance();
			//load balance
			//get the service father. Stored in class LB
			//todo，对每一步的异常都还没有进行考虑
			LoadBalance* lb = LoadBalance::getInstance();
			lb->getMd5ToServiceFather();
			lb->getMonitors();
			lb->balance();

			//after load balance. Each monitor should load the service to Config
			ServiceListener* serviceListener = ServiceListener::getInstance();
			serviceListener->getAllIp();
			serviceListener->loadAllService();

	     	//multiThread module
	     	MultiThread* ml = MultiThread::getInstance(_zk);
	        ml->runMainThread();

	        //It's important !! Remember to close it always
			delete lb;
			delete serviceListener;
			delete ml;
            if (_stop || MultiThread::isThreadError()) {
                break;
            }
		}
		delete _zk;
	}
	LOG(LOG_ERROR, "EXIT main loop!!!");
	return 0;
}
