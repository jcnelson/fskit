CPP   := g++ -Wall -g -fPIC
LIB   := -lpthread -lrt -lfuse -lfskit
INC   := -I/usr/include -I. -I../
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
HEADERS := $(wildcard *.h)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64
LIBINC	:= -L../

FUSE_DEMO = fuse-demo

LIBFSKIT_FUSE = libfskit_fuse.so
LIBFSKIT_FUSE_SO = libfskit_fuse.so.1
LIBFSKIT_FUSE_LIB = libfskit_fuse.so.1.0.1

DESTDIR = /usr/local/lib
INCDIR = /usr/local/include/fskit/fuse

all: fskit_fuse

fskit_fuse: $(OBJ)
	$(CPP) -shared -Wl,-soname,$(LIBFSKIT_FUSE_SO) -o $(LIBFSKIT_FUSE_LIB) $(OBJ) $(LIBINC) $(LIB)
	$(SHELL) -c "if ! test -L $(LIBFSKIT_FUSE_SO); then /bin/ln -s $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO); fi"
	$(SHELL) -c "if ! test -L $(LIBFSKIT_FUSE); then /bin/ln -s $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE); fi"

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: install
install: fskit_fuse
	/bin/cp -a $(LIBFSKIT_FUSE) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE_LIB) $(DESTDIR)
	/bin/mkdir -p $(INCDIR)
	/bin/cp -a $(HEADERS) $(INCDIR)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE)
