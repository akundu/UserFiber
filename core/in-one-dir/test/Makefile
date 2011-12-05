CPP=c++
BUILD_FLAGS=-g -O3 -Wall -Wno-deprecated -DPIPE_NOT_EFD  -D_LARGEFILE64_SOURCE=1 -D_COMPILE64BIT_SOURCE=1 -D_GNU_SOURCE -D_REENTRANT -Dlinux -fPIC
#ARCH=i686
ARCH=x86-64

all:	UFHTTPLoader

UFHTTPLoader.o:	UFHTTPLoader.C
	$(CPP) $(BUILD_FLAGS) -o UFHTTPLoader.o -c UFHTTPLoader.C -I/home/e/include -march=$(ARCH)

UFHTTPLoader:	UFHTTPLoader.o
	$(CPP) $(BUILD_FLAGS) -o UFHTTPLoader UFHTTPLoader.o -L/home/e/lib -lUF -lpthread -march=$(ARCH)

clean: 
	rm *.o UFHTTPLoader
