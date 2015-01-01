
include ../buildconf.mk

LIB   := $(PTHREAD_LIBS) -L../ -lfskit
INC   := $(PTHREAD_CFLAGS) -I../include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

COMMON := common.cpp
COMMON_O := common.o

TESTS := $(patsubst test-%.o,test-%,$(OBJ))

all: $(TESTS)

test-% : test-%.o $(COMMON_O)
	$(CXX) $(CFLAGS) -o $@ $(COMMON_O) $< $(LIB)

%.o : %.c
	$(CXX) $(CFLAGS) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CXX) $(CFLAGS) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	$(RM_RF) $(OBJ) $(TESTS)
