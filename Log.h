#ifndef LOG_H
#define LOG_H
#include <cstdio>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <string>
using namespace std;

//log level
#define LOG_FATAL_ERROR 0
#define LOG_ERROR       1
#define LOG_WARNING     2
#define LOG_NOTICE      3
#define LOG_INFO        4
#define LOG_TRACE       5
#define LOG_DEBUG       6

#define LOG(level, format, ...) Log::printLog(__FILE__, __LINE__, level, format, ## __VA__ARGS__)


class Log{
public:
	static int printLog(const char* fileName, const int line, const int level, const char* format, ...);
	static int init(const int ll);
	static string getLogLevelStr(int n);
	static int checkFile(const int year, const int mon, const int day);
private:
	Log();
	~Log();
	static int logLevel;
	static FILE* fp;
	static pthread_mutex_t mutex;
	static char curLogFileName[128];
	static string logLevelitos[7];
};
#endif