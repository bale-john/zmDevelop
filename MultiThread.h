#ifndef MULTITHREAD_H
#define MULTITHREAD_H
#include <pthread.h>
#include <iostream>
#include <cstdio>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <list>
#include "Util.h"
#include "Config.h"
#include "ConstDef.h"
#include "Log.h"
#include "Process.h"
#include "Zk.h"
#include "LoadBalance.h"
#include "ServiceListener.h"
#include "x86_spinlocks.h"
using namespace std;

int runMainThread(Zk* zk_input, const vector<string>& myServiceFather);

#endif
