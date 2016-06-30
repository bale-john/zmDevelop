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
#include <stdarg.h>


int main(int argc, char** argv){
	Config* conf = Config::getInstance();
	Util::printConfig();
	return 0;
}
