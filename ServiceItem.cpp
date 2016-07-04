#include "ServiceItem.h"
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
//support in_addr. It's a ip struct
#include <netinet/in.h>
using namespace std;

ServiceItem::ServiceItem():
    _serviceFather(""),
    _host(""),
    _port(-1),
    _connRetry(0),
    _connTimeout(3),// default 3 seconds
    _status(STATUS_DOWN) 
{   
    memset(&_addr, 0, sizeof(struct in_addr));
}

ServiceItem::ServiceItem(std::string host, struct in_addr *addr, int port, int connRetry, int timeout, std::string serviceFather, int status):
	_serviceFather(serviceFather),
	_host(host),
	_port(port),
	_connRetry(connRetry),
	_connTimeout(timeout),
	_status(status),
	_addr(addr){

	}

ServiceItem::clear() {
	memset(&_addr, 0, sizeof(struct in_addr));
    _host = "";
    _port = -1;
    _connRetry = 0;
    _zkNodePrefix = "";
    _status = -1;
    _connTimeout = -1;
}

