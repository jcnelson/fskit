CPP    := g++ -Wall -g -fPIC
LIB   := -lpthread -lrt
INC   := -I/usr/include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS

LIBFSKIT_SO := libfskit.so.1
LIBFSKIT_LIB := libfskit.so.1.0.1

all: fskit

fskit: $(OBJ) 
	$(CPP) -shared -Wl,-soname,$(LIBFSKIT_SO) -o $(LIBFSKIT_LIB) $(OBJ) $(LIBINC) $(LIB)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LIBFSKIT_LIB)
