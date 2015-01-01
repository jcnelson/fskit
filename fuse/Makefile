
include ../buildconf.mk

LIB   := $(PTHREAD_LIBS) $(FUSE_LIBS) -L../libfskit -lfskit
INC   := $(PTHREAD_CFLAGS) $(FUSE_CFLAGS) -I../include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
HEADERS := $(wildcard *.h)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64
INCDIR		?= $(INCLUDEDIR)/fskit/fuse

FUSE_DEMO = fuse-demo

LIBFSKIT_FUSE = libfskit_fuse.so
LIBFSKIT_FUSE_SO = libfskit_fuse.so.$(VERSION_MAJOR)
LIBFSKIT_FUSE_LIB = libfskit_fuse.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PC_FILE := fskit_fuse.pc

all: libfskit_fuse $(PC_FILE)

$(PC_FILE):	$(PC_FILE).in
	@cat $< | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@INCLUDEDIR@~$(INCDIR)~g;' | \
		sed -e 's~@VERSION@~$(VERSION)~g; ' | \
		sed -e 's~@LIBS@~$(LIB)~g; ' | \
		sed -e 's~@LIBDIR@~$(LIBDIR)~g; ' | \
	   sed -e 's~@VERSION_MAJOR@~$(VERSION_MAJOR)~g; ' | \
	   sed -e 's~@VERSION_MINOR@~$(VERSION_MINOR)~g; ' | \
	   sed -e 's~@VERSION_PATCH@~$(VERSION_PATCH)~g; '	> $@

libfskit_fuse: $(OBJ)
	$(CXX) $(CXXFLAGS) -shared -Wl,-soname,$(LIBFSKIT_FUSE_SO) -o $(LIBFSKIT_FUSE_LIB) $(OBJ) $(LIB)
	$(LN_SF) $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO)
	$(LN_SF) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE)

libfskit_fuse-install: libfskit_fuse $(PC_FILE)
	$(MKDIR_P) $(DESTDIR)/$(LIBDIR) $(DESTDIR)/$(PKGCONFIGDIR)
	$(CP_A) $(LIBFSKIT_FUSE) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE_LIB) $(DESTDIR)/$(LIBDIR)
	$(CP_A) $(PC_FILE) $(DESTDIR)/$(PKGCONFIGDIR)

libfskit_fuse-dev-install: libfskit_fuse
	$(MKDIR_P) $(DESTDIR)/$(INCDIR)
	$(CP_A) $(HEADERS) $(DESTDIR)/$(INCDIR)

%.o : %.c
	$(CXX) $(CXXFLAGS) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(INC) -c $< $(DEFS)

.PHONY: install
install: libfskit_fuse-install libfskit_fuse-dev-install

.PHONY: clean
clean:
	$(RM_RF) $(OBJ) $(LIBFSKIT_FUSE_LIB) $(LIBFSKIT_FUSE_SO) $(LIBFSKIT_FUSE) $(PC_FILE)
