# Configuration variables
BUILDDIR = build
EXECUTABLE = demo
CC = gcc
STRIP = strip --strip-all
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
LDLIBS = -lm
DEBUG ?= 1

# Set debug and release build flags
ifeq ($(DEBUG),0)
CFLAGS += -Os -DDEMO_PLAYER
else
CFLAGS += -Og -g
endif

# Variables for output and intermediate artifacts
SOURCEDIR = src
TARGET = $(BUILDDIR)/$(EXECUTABLE)
SOURCES = $(wildcard $(SOURCEDIR)/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES:$(SOURCEDIR)/%=$(BUILDDIR)/%))

# Third party library variables
LIB_PREFIX = $(CURDIR)/$(BUILDDIR)/install
CFLAGS += -I$(LIB_PREFIX)/include -L$(LIB_PREFIX)/lib

# SDL2
SDL_DIR = lib/SDL
LDLIBS += -l:libSDL2.a


# This rule is for linking the final executable
$(TARGET): $(LIB_PREFIX)/lib/libSDL2.a $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) $^ $(LDLIBS)
ifeq ($(DEBUG),0)
	$(STRIP) $(TARGET)
endif


# This rule is for compiling our C source files
$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<


# This rule is a check for having fetched git submodules
$(SDL_DIR)/configure:
	@echo Please run git submodule update --init first!
	@echo Check README.md for details.
	@exit 1


# This rule is for building SDL2
$(LIB_PREFIX)/lib/libSDL2.a: | $(SDL_DIR)/configure
	cd $(SDL_DIR) && \
	./configure --prefix=$(LIB_PREFIX) && \
	$(MAKE) CFLAGS="$(CFLAGS)" && \
	$(MAKE) install


.PHONY: clean


clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C $(SDL_DIR) $(MAKECMDGOALS)

