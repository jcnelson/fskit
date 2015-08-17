
include ../buildconf.mk

LIB   := $(PTHREAD_LIBS) $(FUSE_LIBS) -L../libfskit -lfskit -L../fuse -lfskit_fuse
INC   := $(PTHREAD_CFLAGS) $(FUSE_CFLAGS) -I../include -I. -I..
C_SRCS:= $(wildcard *.c)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

FUSE_DEMO = fuse-demo

all:	$(FUSE_DEMO)

$(FUSE_DEMO): $(OBJ)
	$(CC) $(CCFLAGS) -o $@ $(OBJ) $(LIB)

%.o : %.c
	$(CC) $(CCFLAGS) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(FUSE_DEMO)
