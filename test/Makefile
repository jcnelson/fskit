
include ../buildconf.mk

CPP   := g++ -Wall -g -fPIC
LIB   := $(PTHREAD_LIBS) -L../ -lfskit
INC   := $(PTHREAD_CFLAGS) -I../include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64
LIBINC	:= -L../

COMMON := common.cpp
COMMON_O := common.o

TESTS := $(patsubst test-%.o,test-%,$(OBJ))

all: $(TESTS)

test-% : test-%.o $(COMMON_O)
	$(CPP) -o $@ $(COMMON_O) $< $(LIBINC) $(LIB)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	$(RM_RF) $(OBJ) $(TESTS)
