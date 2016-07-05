#!/bin/bash
g++ -c -Wall -I /usr/local/include/zookeeper -lcrypto -lpthread Agent.cpp Config.cpp Util.cpp Log.cpp Process.cpp ServiceItem.cpp Zk.cpp -std=c++11 libzookeeper_mt.a 
#./a.out
