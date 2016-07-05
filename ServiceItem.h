#ifndef SERVICEMAP_H
#define SERVICEMAP_H
#include <map>
#include <cstdio>
#include <string>
#include <iostream>
//support in_addr. It's a ip struct
#include <netinet/in.h>
using namespace std;

class ServiceItem{
private:
    std::string _host;
    //todo not understand well
    struct in_addr _addr;
    //todo why no ip ?
    //std::string _ip;
    int _port;
    int _connRetry;
    int _connTimeout;
    std::string _serviceFather;
    int _status; //online or offline
public:
	ServiceItem(std::string host, struct in_addr *addr, int port, int connRetry, int timeout, std::string serviceFather, int status);
	ServiceItem();
	~ServiceItem();

    void clear();
};
#endif
