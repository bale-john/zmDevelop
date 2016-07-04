#include "ServiceItem.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
//support in_addr. It's a ip struct
#include <netinet/in.h>
#include "ConstDef.h"
#include <cstring>
using namespace std;

ServiceItem::ServiceItem():
    _host(""),
    _port(-1),
    _connRetry(0),
    _connTimeout(3),// default 3 seconds
    _serviceFather(""),
    _status(STATUS_DOWN) 
{   
    memset(&_addr, 0, sizeof(struct in_addr));
}

ServiceItem::ServiceItem(std::string host, struct in_addr *addr, int port, int connRetry, int timeout, std::string serviceFather, int status):
	_host(host),
	_addr(*addr),
	_port(port),
	_connRetry(connRetry),
	_connTimeout(timeout),
	_serviceFather(serviceFather),
	_status(status){

	}

void ServiceItem::clear() {
	memset(&_addr, 0, sizeof(struct in_addr));
    _host = "";
    _port = -1;
    _connRetry = 0;
    _serviceFather = "";
    _status = -1;
    _connTimeout = -1;
}

