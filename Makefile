# qconf-monitor make file

# PARAMS
CC 		= g++
CCFLAGS = -c -Wall -I /usr/local/include/zookeeper 
ZOO_LIB = /usr/local/lib/libzookeeper_mt.a
OBJS = Config.o Agent.o Util.o \
				Process.o Log.o \
				ServiceItem.o Zk.o

#.PHONY
.PHONY : all qconf-monitor clean

all : qconf-monitor clean

qconf-monitor : $(OBJS)
	$(CC) -lcrypto -lpthread -o $@ $^ $(ZOO_LIB)

clean : 
	rm -fr $(OBJS)

# OBJS
$(OBJS) : %.o : %.cpp
	$(CC) $(CCFLAGS) -o $@ $<
