CXXFLAGS=-g -O3 -Wall -Werror -Wno-deprecated -march=x86-64
#CXXFLAGS=-g -Wall -Werror -Wno-deprecated -march=x86-64 -fno-inline
CPPFLAGS=-DPIPE_NOT_EFD -I./include
LDFLAGS=-L./core/lib -lUF -lpthread

.PHONY: all clean core/lib

all: core/lib

core/lib:
	$(MAKE) -C core/src all

clean: 
	$(MAKE) -C core/src clean
