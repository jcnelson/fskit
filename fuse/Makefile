
include ../buildconf.mk

CPP   := g++ -Wall -g -fPIC
LIB   := -lpthread -lrt -lfuse -lfskit
INC   := -I../include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
HEADERS := $(wildcard *.h)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64
LIBINC	:= -L../

FUSE_DEMO = fuse-demo

LIBFSKIT_FUSE = libfskit_fuse.so
LIBFSKIT_FUSE_SO = libfskit_fuse.so.$(VERSION_MAJOR)
LIBFSKIT_FUSE_LIB = libfskit_fuse.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PC_FILE := fskit_fuse.pc

PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/fskit/fuse
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig

all: libfskit_fuse $(PC_FILE)

$(PC_FILE):	$(PC_FILE).in
	@cat $< | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@INCLUDEDIR@~$(INCLUDEDIR)~g;' | \
		sed -e 's~@VERSION@~$(VERSION)~g; ' | \
		sed -e 's~@LIBS@~$(LIB)~g; ' | \
		sed -e 's~@LIBDIR@~$(LIBDIR)~g; ' | \
	   sed -e 's~@VERSION_MAJOR@~$(VERSION_MAJOR)~g; ' | \
	   sed -e 's~@VERSION_MINOR@~$(VERSION_MINOR)~g; ' | \
	   sed -e 's~@VERSION_PATCH@~$(VERSION_PATCH)~g; '	> $@

libfskit_fuse: $(OBJ)
	$(CPP) -shared -Wl,-soname,$(LIBFSKIT_FUSE_SO) -o $(LIBFSKIT_FUSE_LIB) $(OBJ) $(LIBINC) $(LIB)
	$(SHELL) -c "if ! test -L $(LIBFSKIT_FUSE_SO); then /bin/ln -s $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO); fi"
	$(SHELL) -c "if ! test -L $(LIBFSKIT_FUSE); then /bin/ln -s $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE); fi"

libfskit_fuse-install: libfskit_fuse $(PC_FILE)
	mkdir -p $(DESTDIR)/$(LIBDIR) $(DESTDIR)/$(PKGCONFIGDIR)
	cp -a $(LIBFSKIT_FUSE) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE_LIB) $(DESTDIR)/$(LIBDIR)
	cp -a $(PC_FILE) $(DESTDIR)/$(PKGCONFIGDIR)

libfskit_fuse-dev-install: libfskit_fuse
	mkdir -p $(DESTDIR)/$(INCLUDEDIR)
	cp -a $(HEADERS) $(DESTDIR)/$(INCLUDEDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: install
install: libfskit_fuse-install libfskit_fuse-dev-install

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE) $(PC_FILE)
