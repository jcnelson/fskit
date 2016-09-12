
include ../buildconf.mk

LIB   := $(PTHREAD_LIBS) -L../libfskit -lfskit
INC   := $(PTHREAD_CFLAGS) -I../include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := $(DEFS) -D_FILE_OFFSET_BITS=64

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
	rm -f $(OBJ) $(TESTS)
