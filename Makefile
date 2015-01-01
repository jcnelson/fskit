
include ./buildconf.mk

CPP    := g++ -Wall -g -fPIC
INC   := $(PTHREAD_CFLAGS) -Iinclude
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE

LIBS		:= $(PTHREAD_LIBS)

PC_FILE		:= fskit.pc

LIBFSKIT := libfskit.so
LIBFSKIT_SO := libfskit.so.$(VERSION_MAJOR)
LIBFSKIT_LIB := libfskit.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PREFIX		?= /usr
LIBDIR		?= $(PREFIX)/lib
INCLUDEDIR	?= $(PREFIX)/include/fskit
PKGCONFIGDIR	?= $(LIBDIR)/pkgconfig

all: libfskit $(PC_FILE)

$(PC_FILE):	$(PC_FILE).in
	@cat $< | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@INCLUDEDIR@~$(INCLUDEDIR)~g;' | \
		sed -e 's~@VERSION@~$(VERSION)~g; ' | \
		sed -e 's~@LIBS@~$(LIBS)~g; ' | \
		sed -e 's~@LIBDIR@~$(LIBDIR)~g; ' | \
	   sed -e 's~@VERSION_MAJOR@~$(VERSION_MAJOR)~g; ' | \
	   sed -e 's~@VERSION_MINOR@~$(VERSION_MINOR)~g; ' | \
	   sed -e 's~@VERSION_PATCH@~$(VERSION_PATCH)~g; '	> $@

libfskit: $(OBJ)
	$(CPP) -shared -Wl,-soname,$(LIBFSKIT_SO) -o $(LIBFSKIT_LIB) $(OBJ) $(LIBINC) $(LIBS)
	$(LN_SF) $(LIBFSKIT_LIB) $(LIBFSKIT_SO)
	$(LN_SF) $(LIBFSKIT_SO) $(LIBFSKIT)

libfskit-install: libfskit $(PC_FILE)
	$(MKDIR_P) $(DESTDIR)/$(LIBDIR) $(DESTDIR)/$(PKGCONFIGDIR)
	$(CP_A) $(LIBFSKIT) $(LIBFSKIT_SO) $(LIBFSKIT_LIB) $(DESTDIR)/$(LIBDIR)
	$(CP_A) $(PC_FILE) $(DESTDIR)/$(PKGCONFIGDIR)

libfskit-dev-install: libfskit
	$(MKDIR_P) $(DESTDIR)/$(INCLUDEDIR)
	$(CP_A) include/fskit/*.h $(DESTDIR)/$(INCLUDEDIR)

install: libfskit-install libfskit-dev-install $(PC_FILE)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean

clean:
	$(RM_RF) $(OBJ) $(LIBFSKIT_LIB) $(LIBFSKIT_SO) $(LIBFSKIT) $(PC_FILE)
	$(MAKE) -C fuse clean
	$(MAKE) -C demo clean
	$(MAKE) -C test clean
