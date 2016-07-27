#ifndef PROCESS_H
#define PROCESS_H
#include <string>
#include <iostream>
#include <cstdio>
#include "ConstDef.h"
using namespace std;

class Process{
private:
	Process();
	~Process();
    static bool stop;

public:
	static bool isProcessRunning(const string& processName);
	static int daemonize();
	static int processKeepalive(int& childExitStatus, const string pidFile);
	static void sigForward(const int sig);
	static void sigHandler(const int sig);
	static int processFileMsg(const string cmdFile);
    static bool getStop();
    static void setStop();
    static void clearStop();
};
#endif
