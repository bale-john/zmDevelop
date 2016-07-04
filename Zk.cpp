#include "Zk.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
using namespace std;

Zk::Zk():_zh(NULL), _recvTimeout(3000), _zkLogPath(""), _zkHost("") {}
