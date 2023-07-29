# Configuration variables
BUILDDIR = build
EXECUTABLE = demo
CC = gcc
STRIP = strip --strip-all
EXTRA_CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
LDLIBS = -lm -l:libSDL2.a
DEBUG ?= 1

# Set debug and release build flags
ifeq ($(DEBUG),0)
CFLAGS += -Os
LDLIBS += -l:librocket-player.a
else
CFLAGS += -Og -g
EXTRA_CFLAGS += -DDEBUG
LDLIBS += -l:librocket.a
endif

# Variables for output and intermediate artifacts
SOURCEDIR = src
TARGET = $(BUILDDIR)/$(EXECUTABLE)
SOURCES = $(wildcard $(SOURCEDIR)/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES:$(SOURCEDIR)/%=$(BUILDDIR)/%))

# Third party library build variables
LIB_PREFIX = $(CURDIR)/$(BUILDDIR)/install
EXTRA_CFLAGS += -I$(LIB_PREFIX)/include -L$(LIB_PREFIX)/lib
LIBRARIES = $(LIB_PREFIX)/lib/libSDL2.a $(LIB_PREFIX)/lib/librocket.a


# This rule is for linking the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) $(EXTRA_CFLAGS) $^ $(LDLIBS)
ifeq ($(DEBUG),0)
	$(STRIP) $(TARGET)
endif


# This rule is for compiling our C source files
$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c $(LIBRARIES)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<


# This rule is a check for having fetched git submodules
lib/SDL/configure:
	@echo Please run git submodule update --init first!
	@echo Check README.md for details.
	@exit 1


# This rule is for building SDL2
$(LIB_PREFIX)/lib/libSDL2.a: | lib/SDL/configure
	cd lib/SDL && \
	./configure --prefix=$(LIB_PREFIX) && \
	$(MAKE) CFLAGS="$(CFLAGS)" CC=$(CC) && \
	$(MAKE) install


# This rule is for building rocket libraries
$(LIB_PREFIX)/lib/librocket.a: | lib/SDL/configure
	$(MAKE) -C lib/rocket lib/librocket.a lib/librocket-player.a CFLAGS="$(CFLAGS)" CC=$(CC)
	@mkdir -p $(LIB_PREFIX)/lib $(LIB_PREFIX)/include
	cp lib/rocket/lib/*.a $(LIB_PREFIX)/lib
	cp lib/rocket/lib/*.h $(LIB_PREFIX)/include

	
.PHONY: clean


clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C lib/SDL $(MAKECMDGOALS)
	$(MAKE) -C lib/rocket $(MAKECMDGOALS)
