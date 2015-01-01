
include ./buildconf.mk

all:
	$(MAKE) -C libfskit
	$(MAKE) -C test
ifeq ($(ENABLE_FUSE),1)
	$(MAKE) -C fuse
	$(MAKE) -C demo
endif

install:
	$(MAKE) -C libfskit install
ifeq ($(ENABLE_FUSE),1)
	$(MAKE) -C fuse install
endif

clean:
	$(MAKE) -C libfskit clean
	$(MAKE) -C test clean
	$(MAKE) -C fuse clean
	$(MAKE) -C demo clean

.PHONY: all install clean
