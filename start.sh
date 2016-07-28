#!/usr/bin
sudo rm log/*
sudo sh qconf-monitor.sh stop
sudo make 
sudo ./qconf-monitor > /home/zhangguowei/info
