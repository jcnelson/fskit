
include ../buildconf.mk

CPP   := g++ -Wall -g -fPIC
LIB   := -lpthread -lrt -lfuse -lfskit -lfskit_fuse
INC   := -I../include -I. -I..
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64
LIBINC	:= -L../ -L../fuse

FUSE_DEMO = fuse-demo

fuse-demo: $(OBJ)
	$(CPP) -o $(FUSE_DEMO) $(OBJ) $(LIBINC) $(LIB)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(FUSE_DEMO)
