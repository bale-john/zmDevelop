#ifndef PROCESS_H
#define PROCESS_H
#include <string>
#include <iostream>
#include <cstdio>
#include "ConstDef.h"
using namespace std;

class Process{
public:
	static bool isProcessRunning(const string& processName);
	static int daemonize();
	static int processKeepalive(int& childExitStatus, const string pidFile);
	static void sigForward(const int sig);
private:
	Process();
	~Process();
};
#endif