#include <stdarg.h>
#include "Log.h"
#include "ConstDef.h"
#include "Config.h"
#include <string>
#include <errno.h>
using namespace std;

int LOG::logLevel = LOG_WARNNING;
FILE* LOG::fp = NULL;
pthread_mutex_t LOG::mutex;
char LOG::curFileName[128] = {0};
string LOG::logLevelitos[7] = {"FATAL_ERROR", "ERROR", "WARNING", "NOTICE", "INFO", "TRACE", "DEBUG"};

int LOG::init(const int ll) {
	logLevel = ll;
}

string LOG::getLogLevelStr(const int level) {
	return logLevelitos[level];
}

int checkFile(const int year, const int mon, const int day){
	char newName[128] = {0};
	snprintf(newName, sizeof(newName), "%s%s.%4d%2d%2d", logPath.c_str(), 
		logFileNamePrefix.c_str(), year, mon, day);
	if (strcmp(curFileName, newName) == 0) {
		return 0;
	}
	else {
		if (fp) {
			pthread_mutex_lock(&mutex);
			fclose(fp);
			fp = NULL;
			pthread_mutex_unlock(&mutex);
		}
		if (strcmp(curFileName, newName) != 0) {
			pthread_mutex_lock(&mutex);
			strcpy(curFileName, newName);
			fp = fopen(curFileName, "a");
			if (!fp) {
				fprintf(stderr, "Log file open failed. Path %s.  Error No:%s\n", curFileName, strerror(errno));
				pthread_mutex_unlock(&mutex);
				return -1;
			}
			pthread_mutex_unlock(&mutex);
		}
	}
	return 0;
}

int LOG::printLog(const char* fileName, const int line, const int level, const char* format, ...) {
	//判断文件是否已经打开,有必要吗？好像没有，init只是为了把loglevel传进来，并不是说换个文件了这个值就不同了，只有在reload了配置文件之后才会不同吧
	//那么这个操作我觉得在config里面做会更好
	/*
	if (!logFile) {
		init();
	}
	*/
	//如果想要记录的级别数据比配置文件中设置的更高，就不予记录。这相当于配置文件中的记录级别是最高级别，比这个低的都会记录
	if (level > logLevel) {
		return 0;
	}
	string logInfo = getLogLevelStr(level);
	//get the time
	time_t curTime;
	time(&curTime);
	struct* realTime = localtime(curTime);

	char logBuf[1024] = {0};
	int charCount = snprintf(logBuf, sizeof(logBuf), "%4d/%2d/%2d %2d:%2d:%2d - [%s][%s %d] - ",
		realTime->tm_year + 1900, realTime->tm_mon + 1, realTime->tm_mday, realTime->tm_hour, 
		realTime->rm_min, realTime->sec, logInfo.c_str(), fileName, line);

	if (checkFile(realTime->tm_year + 1900, realTime->tm_mon + 1, realTime->tm_mday) != 0) {
		return;
	}
	if (!fp) {
		return -1;
	}
	va_list argPtr;
	va_start(argPtr, format);
	vsnprintf(logBuf + charCount, sizeof(logBuf) - n, format, argPtr);
	va_end(argPtr);
	strcat(logBuf, '\n');
	pthread_mutex_lock(&mutex);
	fwrite(logBuf, sizeof(char), strlen(logBuf), fp);
	fflush(fp)
	pthread_mutex_unlock(&mutex);
	return 0;
}