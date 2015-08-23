
include ../buildconf.mk

C_SRCS:= $(wildcard *.c)
OBJ   := $(patsubst %.c,$(BUILD_LIBFSKIT_FUSE)/%.o,$(C_SRCS))
LIBS  := -lpthread -lfuse
DEFS  := $(DEFS) -D_FILE_OFFSET_BITS=64

PC_FILE		:= $(BUILD_PKGCONFIG)/fskit_fuse.pc
PC_FILE_INSTALL := $(PKGCONFIGDIR)/fskit_fuse.pc
PC_FILE_IN  := fskit_fuse.pc.in

LIBFSKIT_FUSE := libfskit_fuse.so
LIBFSKIT_FUSE_SO := libfskit_fuse.so.$(LIBFSKIT_FUSE_MAJOR)
LIBFSKIT_FUSE_LIB := libfskit_fuse.so.$(LIBFSKIT_FUSE_MAJOR).$(LIBFSKIT_FUSE_MINOR).$(LIBFSKIT_FUSE_PATCH)

LIBFSKIT_FUSE_TARGET := $(patsubst %,$(BUILD_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE))
LIBFSKIT_FUSE_SO_TARGET := $(patsubst %,$(BUILD_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_SO))
LIBFSKIT_FUSE_LIB_TARGET := $(patsubst %,$(BUILD_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_LIB))

LIBFSKIT_FUSE_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT_FUSE)/%,$(INSTALL_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_TARGET))
LIBFSKIT_FUSE_SO_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT_FUSE)/%,$(INSTALL_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_SO_TARGET))
LIBFSKIT_FUSE_LIB_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT_FUSE)/%,$(INSTALL_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_LIB_TARGET))

LIBFSKIT_FUSE_INSTALL: $(patsubst $(BUILD_LIBFSKIT_FUSE)/%,$(INSTALL_LIBFSKIT_FUSE)/%,$(LIBFSKIT_FUSE_TARGET) $(LIBFSKIT_FUSE_SO_TARGET) $(LIBFSKIT_FUSE_LIB_TARGET))

HEADERS := $(wildcard *.h)
BUILD_HEADERS := $(patsubst %.h,$(BUILD_LIBFSKIT_FUSE_HEADERS)/%.h,$(HEADERS))
INSTALL_HEADERS := $(patsubst $(BUILD_LIBFSKIT_FUSE_HEADERS)/%.h,$(INSTALL_LIBFSKIT_FUSE_HEADERS)/%.h,$(BUILD_HEADERS))

all: $(LIBFSKIT_FUSE_TARGET) $(LIBFSKIT_FUSE_SO_TARGET) $(LIBFSKIT_FUSE_LIB_TARGET) $(BUILD_HEADERS) $(PC_FILE)

$(PC_FILE):	$(PC_FILE_IN)
	@mkdir -p "$(shell dirname "$@")"
	@cat "$<" | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@INCLUDEDIR@~$(INCLUDEDIR)~g;' | \
		sed -e 's~@VERSION@~$(VERSION)~g; ' | \
		sed -e 's~@LIBS@~$(LIBS)~g; ' | \
		sed -e 's~@LIBDIR@~$(LIBDIR)~g; ' | \
	   sed -e 's~@VERSION_MAJOR@~$(LIBFSKIT_FUSE_MAJOR)~g; ' | \
	   sed -e 's~@VERSION_MINOR@~$(LIBFSKIT_FUSE_MINOR)~g; ' | \
	   sed -e 's~@VERSION_PATCH@~$(LIBFSKIT_FUSE_PATCH)~g; '	> "$@"

$(LIBFSKIT_FUSE_LIB_TARGET): $(OBJ)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CCFLAGS) -shared -Wl,-soname,$(LIBFSKIT_FUSE_SO) -o $(LIBFSKIT_FUSE_LIB_TARGET) $(OBJ) $(LIBINC) $(LIBS)

$(LIBFSKIT_FUSE_SO_TARGET): $(LIBFSKIT_FUSE_LIB_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(LIBFSKIT_FUSE_TARGET): $(LIBFSKIT_FUSE_SO_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(BUILD_LIBFSKIT_FUSE_HEADERS)/%.h: %.h
	@mkdir -p "$(shell dirname "$@")"
	@cp -a "$<" "$@"

$(BUILD_LIBFSKIT_FUSE)/%.o : %.c $(BUILD_HEADERS)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CCFLAGS) -o "$@" $(INC) -c "$<" $(DEFS)


install: $(LIBFSKIT_FUSE_INSTALL_TARGET) $(LIBFSKIT_FUSE_SO_INSTALL_TARGET) $(LIBFSKIT_FUSE_LIB_INSTALL_TARGET) $(PC_FILE_INSTALL) $(INSTALL_HEADERS)

$(LIBFSKIT_FUSE_INSTALL_TARGET): $(LIBFSKIT_FUSE_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(LIBFSKIT_FUSE_SO_INSTALL_TARGET): $(LIBFSKIT_FUSE_SO_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(LIBFSKIT_FUSE_LIB_INSTALL_TARGET): $(LIBFSKIT_FUSE_LIB_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(PC_FILE_INSTALL): $(PC_FILE)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(INSTALL_LIBFSKIT_FUSE_HEADERS)/%.h: $(BUILD_LIBFSKIT_FUSE_HEADERS)/%.h
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

.PHONY: clean
clean:
	rm -f $(OBJ) $(LIBFSKIT_FUSE_TARGET) $(LIBFSKIT_FUSE_SO_TARGET) $(LIBFSKIT_FUSE_LIB_TARGET) $(BUILD_HEADERS) $(PC_FILE)

.PHONY: uninstall
uninstall:
	rm -f $(LIBFSKIT_FUSE_INSTALL_TARGET) $(LIBFSKIT_FUSE_SO_INSTALL_TARGET) $(LIBFSKIT_FUSE_LIB_INSTALL_TARGET) $(INSTALL_HEADERS) $(PC_FILE_INSTALL)

print-%: ; @echo $*=$($*)
