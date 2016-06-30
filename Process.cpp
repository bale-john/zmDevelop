#include <string>
#include <iostream>
#include <cstdio>
#include "Process.h"
#include <cstring>
#include "Log.h"
using namespace std;

bool Process::isProcessRunning(const string& processName) {
	FILE* ptr = NULL;
	char ps[128] = {0};
	char resBuf[128] = {0};
	snprintf(ps, sizeof(ps), "ps -e | grep -c %s", processName.c_str());
	strcpy(resBuf, "ABNORMAL");
	if ((ptr = popen(ps, "r")) != NULL) {
		while(fgets(resBuf, 128, ptr)) {
			if (stoi(resBuf) >= 2) {
				pclose(ptr);
				return true;
			}
		}
		if (strcmp(resBuf, "ABNORMAL") != 0) {
			return false;
		}
	}
	//excute ps failed or fgets() failed
	LOG(LOG_ERROR, "excute command failed");
	return true;
}