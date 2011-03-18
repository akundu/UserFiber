CPP=c++
BUILD_FLAGS=-g -O3 -Wall -Werror -DPIPE_NOT_EFD -Wno-deprecated
#BUILD_FLAGS=-g -Wall -Werror -Wno-deprecated -fno-inline
#BUILD_FLAGS=-g -Wall -Werror -Wno-deprecated 
ARCH=x86-64
INCLUDE=-I../core/include
ARESDIR=../core/ext
ARES_VERSION = 1.7.1
ARES = c-ares-$(ARES_VERSION)
ARES_INCLUDE = -I$(ARESDIR)/$(ARES)
#ARES_LIB = $(ARESDIR)/$(ARES)/.libs/libcares.a 
#ARES_LIB = -L/usr/lib/ -lcares

all:	UFHTTPLoader echoServer UFSleepTest UFCondTimedWaitTest httpProxy testSignal testSleep UFTestConnPool ufTestHTTPServer testProducer ufTestHTTPServerPC testConf
#all:	testSignal
#all:	httpProxy UFHTTPLoader
#all:	testProducer testSignal
#all:	testSignal
#all:	UFHTTPLoader

UFHTTPLoader.o:	UFHTTPLoader.C
	$(CPP) $(BUILD_FLAGS) -c -o UFHTTPLoader.o UFHTTPLoader.C $(INCLUDE) -march=$(ARCH)

UFHTTPLoader:	UFHTTPLoader.o
	$(CPP) $(BUILD_FLAGS) -o UFHTTPLoader UFHTTPLoader.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

ufTestHTTPServerPC.o:	ufTestHTTPServerPC.C
	$(CPP) $(BUILD_FLAGS) -c -o ufTestHTTPServerPC.o ufTestHTTPServerPC.C $(INCLUDE) -march=$(ARCH)

ufTestHTTPServerPC:	ufTestHTTPServerPC.o
	$(CPP) $(BUILD_FLAGS) -o ufTestHTTPServerPC ufTestHTTPServerPC.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

ufTestHTTPServer.o:	ufTestHTTPServer.C
	$(CPP) $(BUILD_FLAGS) -c -o ufTestHTTPServer.o ufTestHTTPServer.C $(INCLUDE) -march=$(ARCH)

ufTestHTTPServer:	ufTestHTTPServer.o
	$(CPP) $(BUILD_FLAGS) -o ufTestHTTPServer ufTestHTTPServer.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

UFTestConnPool.o:	UFTestConnPool.C
	$(CPP) $(BUILD_FLAGS) -c -o UFTestConnPool.o UFTestConnPool.C $(INCLUDE) -march=$(ARCH) 

UFTestConnPool:	UFTestConnPool.o
	$(CPP) $(BUILD_FLAGS) -o UFTestConnPool UFTestConnPool.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

echoServer.o:	echoServer.C
	$(CPP) $(BUILD_FLAGS) -c -o echoServer.o echoServer.C $(INCLUDE) -march=$(ARCH)

echoServer:	echoServer.o
	$(CPP) $(BUILD_FLAGS) -o echoServer echoServer.o -L../core/lib/ -lUF -lpthread -march=$(ARCH) 

httpProxy.o:	httpProxy.C
	$(CPP) $(BUILD_FLAGS) -c -o httpProxy.o httpProxy.C $(INCLUDE) -march=$(ARCH)

httpProxy:	httpProxy.o
	$(CPP) $(BUILD_FLAGS) -o httpProxy httpProxy.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

UFSleepTest.o:	UFSleepTest.C
	$(CPP) $(BUILD_FLAGS) -c -o UFSleepTest.o UFSleepTest.C $(INCLUDE) -march=$(ARCH)

UFSleepTest:	UFSleepTest.o
	$(CPP) $(BUILD_FLAGS) -o UFSleepTest UFSleepTest.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

testProducer.o:	testProducer.C
	$(CPP) $(BUILD_FLAGS) -c -o testProducer.o testProducer.C $(INCLUDE) -march=$(ARCH)

testProducer:	testProducer.o
	$(CPP) $(BUILD_FLAGS) -o testProducer testProducer.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

testSignal.o:	testSignal.C
	$(CPP) $(BUILD_FLAGS) -c -o testSignal.o testSignal.C $(INCLUDE) -march=$(ARCH)

testSignal:	testSignal.o
	$(CPP) $(BUILD_FLAGS) -o testSignal testSignal.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

testSleep.o:	testSleep.C
	$(CPP) $(BUILD_FLAGS) -c -o testSleep.o testSleep.C $(INCLUDE) -march=$(ARCH)

testSleep:	testSleep.o
	$(CPP) $(BUILD_FLAGS) -o testSleep testSleep.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

UFCondTimedWaitTest.o:	UFCondTimedWaitTest.C
	$(CPP) $(BUILD_FLAGS) -c -o UFCondTimedWaitTest.o UFCondTimedWaitTest.C $(INCLUDE) -march=$(ARCH)

UFCondTimedWaitTest:	UFCondTimedWaitTest.o
	$(CPP) $(BUILD_FLAGS) -o UFCondTimedWaitTest UFCondTimedWaitTest.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

testConf.o:	testConf.C
	$(CPP) $(BUILD_FLAGS) -c -o testConf.o testConf.C $(INCLUDE) -march=$(ARCH)

testConf:	testConf.o
	$(CPP) $(BUILD_FLAGS) -o testConf testConf.o -L../core/lib/ -lUF -lpthread -march=$(ARCH)

clean: 
	rm -f *.o UFHTTPLoader echoServer UFSleepTest UFCondTimedWaitTest httpProxy testSignal testSleep testConf
