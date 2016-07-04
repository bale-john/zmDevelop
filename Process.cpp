#include <string>
#include <iostream>
#include <cstdio>
#include "Process.h"
#include <cstring>
#include "Log.h"
#include "Util.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

using namespace std;
//whather process is stopped ?
static bool _stop = false;

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
    //here close all the file description and redirect stand IO
    fd = open("/dev/null", O_RDWR, 0);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    dtablesize = getdtablesize();
    for (fd = 3; fd < dtablesize; ++fd) {
    	close(fd);
    }
    umask(0);
    return 0;
}

void Process::sigForward(const int sig) {
    signal(sig, SIG_IGN);
    kill(0, sig);
}

int Process::processFileMsg(const string cmdFile) {

    return 0;
}

void Process::sigHandler(const int sig) {
    switch (sig) {
        case SIGTERM:
            LOG(LOG_INFO, "Receive signal SIGTERM");
            _stop = true;
            break;
        case SIGKILL:
            LOG(LOG_INFO, "Receive signal SIGKILL");
            _stop = true;
            break;
        case SIGINT:
            LOG(LOG_INFO, "Receive signal SIGINT");
            break;
        case SIGUSR1:
            LOG(LOG_INFO, "Receive signal SIGUSR1");
            processFileMsg(CMDFILE);
            break;
        case SIGUSR2:
            LOG(LOG_INFO, "Receive signal SIGUSR2");
            _stop = true;
            break;
        default:
            break;
    }
}

int Process::processKeepalive(int& childExitStatus, const string pidFile) {

    int processNum = 0;
    pid_t childPid = -1;

    while (1) {
        while (processNum < 1) {
            childPid = fork();
            if (childPid < 0) {
                LOG(LOG_FATAL_ERROR, "fork excute failed");
                return -1;
            }
            //child process
            else if (childPid == 0) {
                LOG(LOG_INFO, "child process ID %d", getpid());
                signal(SIGTERM, sigHandler);
                signal(SIGKILL, sigHandler);
                signal(SIGUSR1, sigHandler);
                signal(SIGUSR2, sigHandler);
                return 0;
            }
            //parent process
            else {
                ++processNum;
                LOG(LOG_INFO, "try to keep PID = %d alive", childPid);
                //todo write failes and something like that
                int ret = Util::writeToFile(to_string(childPid), pidFile);

                signal(SIGINT, sigForward);
                signal(SIGTERM, sigForward);
                signal(SIGHUP, sigForward);
                signal(SIGUSR1, sigForward);
                signal(SIGUSR2, sigForward);
            }
        }
        //parent process 
        LOG(LOG_INFO, "waiting for PID = %d", childPid);
        struct rusage resourceUsage;
        int exitPid = -1;
        int exitStatus = -1;
#ifdef HAVE_WAIT4
        exitPid = wait4(childPid, &exitStatus, 0, resourceUsage);
#else
        memset(&resourceUsage, 0, sizeof(resourceUsage));
        exitPid = waitpid(childPid, &exitStatus, 0);
#endif
        LOG(LOG_INFO, "child process %d returned %d", childPid, exitPid);

        if (childPid == exitPid) {
            //todo delete pid file or clear it?

            //正常退出。但到底怎么定义正常退出？
            if (WIFEXITED(exitStatus)) {
                LOG(LOG_INFO, "worker process PID = %d exited normally with exit-code = %d (it used %ld kBytes max",
                    childPid, WEXITSTATUS(exitStatus), resourceUsage.ru_maxrss / 1024);
                childExitStatus = WEXITSTATUS(exitStatus);
                return 1;
            }
            //因为收到信号退出，比如用户kill了这个进程，那么父进程应该会自动再启动它
            else if (WIFSIGNALED(exitStatus)) {
                LOG(LOG_INFO, "worker process PID = %d died on signal=%d (it used %ld kBytes max) ",
                    childPid, WTERMSIG(exitStatus), resourceUsage.ru_maxrss / 1024);
                int timeToWait = 2;
                while (timeToWait > 0) {
                    timeToWait = sleep(timeToWait);
                }
                --processNum;
                childPid = -1;
                signal(SIGINT, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                signal(SIGHUP, SIG_DFL);
            }
            else if (WIFSTOPPED(exitStatus)) {
                LOG(LOG_INFO, "child process is stopped and should restart later");
            }
        }
        else if (exitPid == -1) {
            if (errno != EINTR) {
                /* how can this happen ? */
                LOG(LOG_INFO, "wait4(%d, ...) failed. errno: %d", childPid, errno);
                return -1; 
            }
        }
    } 
}
