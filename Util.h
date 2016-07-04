#ifndef UTIL_H
#define UTIL_H
#include <string>
#include <vector>
//#include "Config.h"
using namespace std;


class Util{
public:
	Util();
	~Util();
	static int trim(string&);
	static vector<string> split(const string& str, const char separator);
	static int printConfig();
	static int writeToFile(const string content, const string file);
};
#endif