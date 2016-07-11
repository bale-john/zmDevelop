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
static bool _stop = false;

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

		// init zookeeper handler
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

		/******************************
		// haven't consider watcher!!! 
		******************************/

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

		//load balance
		//get the service father. Stored in class LB
		//新建一个负载均衡实例，然后需要填充这个实例中一些重要的数据
		//todo，对每一步的异常都还没有进行考虑
		LoadBalance* lb = new LoadBalance();
		lb->getMd5ToServiceFather();
		lb->getMonitors();
		lb->balance();

		//after load balance. Each monitor should load the service to Config
		//此处我是否需要新建一个类来做监视工作呢？好像是要的吧，lb的watcher和这个应该还是不同的，先新建一个吧
		ServiceListener* serviceListener = new ServiceListener();
		serviceListener->getAllIp(lb->getMyServiceFather());
		//这里如何加锁也都还没考虑，因为加了watch之后(?)可能会有不止一个线程在操作的数据结构都需要加锁，目前还没有考虑，最后统一加吧
		serviceListener->loadAllService();
        cout << "come to multiThread" << endl;

		//load service complete. So can do multithread module?
        runMainThread(_zk, lb->getMyServiceFather());
        while (1){}

        //seems it's important !! Remember to close it always
		delete lb;
		delete serviceListener;
        zookeeper_close(_zk->_zh);
	}

    cout << "EXIT!!!" << endl;
	return 0;
}
