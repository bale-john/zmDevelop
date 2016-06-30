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
using namespace std;

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
	return 0;
}
