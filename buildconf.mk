
VERSION_MAJOR	:= 1
VERSION_MINOR	:= 0
VERSION_PATCH	:= 1
VERSION		:= $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PKG_CONFIG	?= pkg-config

PTHREAD_LIBS	?= -lpthread -lrt
PTHREAD_CFLAGS	?=

FUSE_LIBS	?= `$(PKG_CONFIG) --libs fuse`
FUSE_CFLAGS	?= `$(PKG_CONFIG) --cflags fuse`

LN		?= ln
LN_SF		?= $(LN) -sf
RM		?= rm
RM_RF		?= $(RM) -Rf
MKDIR		?= mkdir
MKDIR_P		?= $(MKDIR) -p
CP		?= cp
CP_A		?= $(CP) -a
