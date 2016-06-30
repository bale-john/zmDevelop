#ifndef PROCESS_H
#define PROCESS_H
#include <string>
#include <iostream>
#include <cstdio>
using namespace std;

class Process{
public:
	static bool isProcessRunning(const string& processName);
	static int daemonize();
private:
	Process();
	~Process();
};
#endif