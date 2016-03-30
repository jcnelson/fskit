# build environment
ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD    ?= $(ROOT_DIR)/build
VERSION  ?= 0.2
OS ?= LINUX
BUILD_BINDIR := $(BUILD)/bin
BUILD_SBINDIR := $(BUILD)/sbin
BUILD_LIBDIR := $(BUILD)/lib
BUILD_USRBIN := $(BUILD)/usr/bin
BUILD_USRLIB := $(BUILD)/usr/lib
BUILD_USRSHARE := $(BUILD)/usr/share
BUILD_PKGCONFIG := $(BUILD)/usr/lib/pkgconfig
BUILD_USRSBIN := $(BUILD)/usr/sbin
BUILD_INCLUDEDIR := $(BUILD)/usr/include/
BUILD_PRIVATE_INCLUDEDIR := $(BUILD)/fskit_private
BUILD_ETCDIR := $(BUILD)/etc

# install environment
DESTDIR        ?= ""
PREFIX         ?= /usr/local
INCLUDE_PREFIX ?= $(PREFIX)
BINDIR         ?= $(DESTDIR)$(PREFIX)/bin
SBINDIR			?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR         ?= $(DESTDIR)$(PREFIX)/lib
INCLUDEDIR     ?= $(DESTDIR)$(INCLUDE_PREFIX)/include
PKGCONFIGDIR   ?= $(DESTDIR)$(PREFIX)/lib/pkgconfig
ETCDIR			?= $(DESTDIR)$(PREFIX)/etc

# libfskit 
BUILD_LIBFSKIT := $(BUILD_USRLIB)
BUILD_LIBFSKIT_HEADERS := $(BUILD_INCLUDEDIR)/fskit
BUILD_LIBFSKIT_DIRS := $(BUILD_LIBFSKIT) $(BUILD_LIBFSKIT_HEADERS)
INSTALL_LIBFSKIT := $(LIBDIR)
INSTALL_LIBFSKIT_HEADERS := $(INCLUDEDIR)/fskit
LIBFSKIT_MAJOR := 1
LIBFSKIT_MINOR := 0
LIBFSKIT_PATCH := 2

# libfskit_fuse
BUILD_LIBFSKIT_FUSE := $(BUILD_USRLIB)
BUILD_LIBFSKIT_FUSE_HEADERS := $(BUILD_INCLUDEDIR)/fskit/fuse
BUILD_LIBFSKIT_FUSE_DIRS := $(BUILD_LIBFSKIT_FUSE) $(BUILD_LIBFSKIT_FUSE_HEADERS)
INSTALL_LIBFSKIT_FUSE := $(LIBDIR)
INSTALL_LIBFSKIT_FUSE_HEADERS := $(INCLUDEDIR)/fskit/fuse
LIBFSKIT_FUSE_MAJOR := 1
LIBFSKIT_FUSE_MINOR := 0
LIBFSKIT_FUSE_PATCH := 2

# special defs
REPL_DEF := 
ifeq ($(REPL),1)
   REPL_DEF := -D_FSKIT_REPL
endif

# compiler
CCFLAGS     := -Wall -std=c11 -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
CXXFLAGS   := -Wall -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
INC      := -I. -I$(ROOT_DIR) -I$(BUILD_INCLUDEDIR) -I$(BUILD)
DEFS     := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS $(REPL_DEF)
LIBINC   := -L. -L$(BUILD_LIBDIR)
CC       ?= cc
CXX      ?= c++

# build setup
BUILD_DIRS   := $(sort $(BUILD_LIBFSKIT_DIRS) $(BUILD_LIBFSKIT_FUSE_DIRS) $(BUILD_PRIVATE_INCLUDEDIR)) 

all:

build_setup: $(BUILD_DIRS)

$(BUILD_DIRS):
	@mkdir -p "$@"

# rule to make an archive member from an object file
(%.o): %.o ;    $(AR) cr $@ $*.o

# debugging...
print-%: ; @echo $*=$($*)
