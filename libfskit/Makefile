
include ../buildconf.mk

C_SRCS:= $(wildcard *.c)
OBJ   := $(patsubst %.c,$(BUILD_LIBFSKIT)/%.o,$(C_SRCS))
LIBS  := -lpthread

PC_FILE		:= $(BUILD_PKGCONFIG)/fskit.pc
PC_FILE_INSTALL := $(PKGCONFIGDIR)/fskit.pc
PC_FILE_IN  := fskit.pc.in

LIBFSKIT := libfskit.so
LIBFSKIT_SO := libfskit.so.$(LIBFSKIT_MAJOR)
LIBFSKIT_LIB := libfskit.so.$(LIBFSKIT_MAJOR).$(LIBFSKIT_MINOR).$(LIBFSKIT_PATCH)

LIBFSKIT_TARGET := $(patsubst %,$(BUILD_LIBFSKIT)/%,$(LIBFSKIT))
LIBFSKIT_SO_TARGET := $(patsubst %,$(BUILD_LIBFSKIT)/%,$(LIBFSKIT_SO))
LIBFSKIT_LIB_TARGET := $(patsubst %,$(BUILD_LIBFSKIT)/%,$(LIBFSKIT_LIB))

LIBFSKIT_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT)/%,$(INSTALL_LIBFSKIT)/%,$(LIBFSKIT_TARGET))
LIBFSKIT_SO_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT)/%,$(INSTALL_LIBFSKIT)/%,$(LIBFSKIT_SO_TARGET))
LIBFSKIT_LIB_INSTALL_TARGET := $(patsubst $(BUILD_LIBFSKIT)/%,$(INSTALL_LIBFSKIT)/%,$(LIBFSKIT_LIB_TARGET))

# LIBFSKIT_INSTALL: $(patsubst $(BUILD_LIBFSKIT)/%,$(INSTALL_LIBFSKIT)/%,$(LIBFSKIT_TARGET) $(LIBFSKIT_SO_TARGET) $(LIBFSKIT_LIB_TARGET))

HEADERS := $(wildcard ../include/fskit/*.h)
PRIVATE_HEADERS := $(wildcard ../include/fskit_private/*.h)

BUILD_HEADERS := $(patsubst ../include/fskit/%.h,$(BUILD_LIBFSKIT_HEADERS)/%.h,$(HEADERS))
BUILD_PRIVATE_HEADERS := $(patsubst ../include/fskit_private/%.h,$(BUILD_PRIVATE_INCLUDEDIR)/%.h,$(PRIVATE_HEADERS))

INSTALL_HEADERS := $(patsubst $(BUILD_LIBFSKIT_HEADERS)/%.h,$(INSTALL_LIBFSKIT_HEADERS)/%.h,$(BUILD_HEADERS))

all: $(LIBFSKIT_TARGET) $(LIBFSKIT_SO_TARGET) $(LIBFSKIT_LIB_TARGET) $(BUILD_HEADERS) $(BUILD_PRIVATE_HEADERS) $(PC_FILE)

$(PC_FILE):	$(PC_FILE_IN)
	@mkdir -p "$(shell dirname "$@")"
	@cat "$<" | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@INCLUDEDIR@~$(INCLUDEDIR)~g;' | \
		sed -e 's~@VERSION@~$(VERSION)~g; ' | \
		sed -e 's~@LIBS@~$(LIBS)~g; ' | \
		sed -e 's~@LIBDIR@~$(LIBDIR)~g; ' | \
	   sed -e 's~@VERSION_MAJOR@~$(LIBFSKIT_MAJOR)~g; ' | \
	   sed -e 's~@VERSION_MINOR@~$(LIBFSKIT_MINOR)~g; ' | \
	   sed -e 's~@VERSION_PATCH@~$(LIBFSKIT_PATCH)~g; '	> "$@"

$(LIBFSKIT_LIB_TARGET): $(OBJ)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CCFLAGS) -shared -Wl,-soname,$(LIBFSKIT_SO) -o $(LIBFSKIT_LIB_TARGET) $(OBJ) $(LIBINC) $(LIBS)

$(LIBFSKIT_SO_TARGET): $(LIBFSKIT_LIB_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(LIBFSKIT_TARGET): $(LIBFSKIT_SO_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(BUILD_PRIVATE_INCLUDEDIR)/%.h: ../include/fskit_private/%.h
	@mkdir -p "$(shell dirname "$@")"
	@cp -a "$<" "$@"

$(BUILD_LIBFSKIT_HEADERS)/%.h: ../include/fskit/%.h
	@mkdir -p "$(shell dirname "$@")"
	@cp -a "$<" "$@"

$(BUILD_LIBFSKIT)/%.o : %.c $(BUILD_HEADERS) $(BUILD_PRIVATE_HEADERS)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CCFLAGS) -o "$@" $(INC) -c "$<" $(DEFS)

$(BUILD_LIBFSKIT)/%.o : %.cpp $(BUILD_HEADERS) $(BUILD_PRIVATE_HEADERS)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CCFLAGS) -o "$@" $(INC) -c "$<" $(DEFS)

libs-install: $(LIBFSKIT_INSTALL_TARGET) $(LIBFSKIT_SO_INSTALL_TARGET) $(LIBFSKIT_LIB_INSTALL_TARGET)

headers-install: $(PC_FILE_INSTALL) $(INSTALL_HEADERS)

install: libs-install headers-install

$(LIBFSKIT_INSTALL_TARGET): $(LIBFSKIT_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(LIBFSKIT_SO_INSTALL_TARGET): $(LIBFSKIT_SO_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(LIBFSKIT_LIB_INSTALL_TARGET): $(LIBFSKIT_LIB_TARGET)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(PC_FILE_INSTALL): $(PC_FILE)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(INSTALL_LIBFSKIT_HEADERS)/%.h: $(BUILD_LIBFSKIT_HEADERS)/%.h
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

.PHONY: clean
clean:
	rm -f $(OBJ) "$(LIBFSKIT_TARGET)" "$(LIBFSKIT_SO_TARGET)" "$(LIBFSKIT_LIB_TARGET)" $(BUILD_HEADERS) $(BUILD_PRIVATE_HEADERS) "$(PC_FILE)"

.PHONY: uninstall
uninstall:
	rm -f "$(LIBFSKIT_INSTALL_TARGET)" "$(LIBFSKIT_SO_INSTALL_TARGET)" "$(LIBFSKIT_LIB_INSTALL_TARGET)" $(INSTALL_HEADERS) "$(PC_FILE_INSTALL)"

print-%: ; @echo $*=$($*)
