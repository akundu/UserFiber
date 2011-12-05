CPP=c++
BUILD_FLAGS=-g -O3 -Wall -Wno-deprecated -DPIPE_NOT_EFD  -D_LARGEFILE64_SOURCE=1 -D_COMPILE64BIT_SOURCE=1 -D_GNU_SOURCE -D_REENTRANT -Dlinux -fPIC
#ARCH=i686
ARCH=x86-64

all:	libUF.so
#all:	UFCache UFLockTest UFTestConnPool

UF.o:	UF.C UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UF.o UF.C -march=$(ARCH)

UFIO.o:	UFIO.C UFIO.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFIO.o UFIO.C -march=$(ARCH)

UFStats.o:	UFStats.C UFIO.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFStats.o UFStats.C -march=$(ARCH)

UFStatSystem.o:	UFStatSystem.C UFStats.H UFIO.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFStatSystem.o UFStatSystem.C -march=$(ARCH)

UFConnectionPoolImpl.o:	UFConnectionPoolImpl.C UFIO.H UF.H UFStats.H
	$(CPP) $(BUILD_FLAGS) -c -o UFConnectionPoolImpl.o UFConnectionPoolImpl.C -march=$(ARCH)

UFPC.o:	UFPC.C UFPC.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFPC.o UFPC.C -march=$(ARCH)

UFServer.o:	UFServer.C UFServer.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFServer.o UFServer.C -march=$(ARCH)

UFConf.o:	UFConf.C UFConf.H UFIO.H UF.H
	$(CPP) $(BUILD_FLAGS) -c -o UFConf.o UFConf.C -march=$(ARCH)

UFSwapContext.o:	UFSwapContext.S
	$(CPP) $(BUILD_FLAGS) -o UFSwapContext.o -c UFSwapContext.S -march=$(ARCH)

libUF.so:	UFIO.o UF.o UFConnectionPoolImpl.o UFConf.o UFStats.o UFPC.o UFStatSystem.o UFServer.o UFSwapContext.o
	$(CPP) -shared $(BUILD_FLAGS) -o libUF.so UFSwapContext.o UFConf.o UFServer.o UFPC.o UFStatSystem.o UFConnectionPoolImpl.o UFStats.o UFIO.o UF.o -L/usr/local/lib -lpthread -march=$(ARCH)

install: all
	cp libUF.so /home/e/lib/
	cp *H /home/e/include/UF/core/

clean: 
	rm *.o *.so
