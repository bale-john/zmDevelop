#include <string>
#include <iostream>
#include <cstdio>
#include "Process.h"
#include <cstring>
#include "Log.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

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

int Process::daemonize() {
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    int fd, dtablesize;
    pid_t pid;
    //already a daemon
    if (getppid() == 1) {
    	return 1;
    }
    //fork off the parent process
    pid = fork();
    if (pid < 0) {
    	exit(1);
    }
    else if (pid > 0) {
    	exit(0);
    }
    //create a new session ID
    if (setsid() < 0) {
    	exit(1);
    }

    pid = fork();
    if (pid < 0) {
    	exit(1);
    }
    else if (pid > 0) {
    	exit(0);
    }
    //change directory, seems we don't need it
    /*
    if (chdir("/") < 0) {
    	exit(EXIT_FAILURE);
    }*/
    //here close all the file description and redirect stang IO
    fd = open("/dev/null", O_RDWR, 0);
    dup2(fd, STDIN_FILENO)
    dup2(fd, STDOUT_FILENO)
    dup2(fd, STDERR_FILENO)
    dtablesize = getdtablesize();
    for (fd = 3; fd < dtablesize; ++fd) {
    	close(fd);
    }
    umask(0);
    return 0;
}