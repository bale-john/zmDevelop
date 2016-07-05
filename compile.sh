#!/bin/bash
g++ -c -Wall -I /usr/local/include/zookeeper -lcrypto -lpthread Agent.cpp Config.cpp Util.cpp Log.cpp Process.cpp ServiceItem.cpp Zk.cpp -std=c++11 /usr/local/lib/libzookeeper_mt.a 
#./a.out
