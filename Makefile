# Configuration variables
BUILDDIR = build
RELEASEDIR = release
EXECUTABLE = demo
CC = gcc
STRIP = strip --strip-all
EXTRA_CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -DGL_GLEXT_PROTOTYPES -I$(BUILDDIR)/include -L$(BUILDDIR)/lib
LDLIBS = -lm -l:libSDL2.a -lGL
DEBUG ?= 1

# Set debug and release build flags
ifeq ($(DEBUG),0)
OBJDIR=$(RELEASEDIR)
CFLAGS += -Os
EXTRA_CFLAGS += -DSYNC_PLAYER
LDLIBS += -l:librocket-player.a
else
OBJDIR=$(BUILDDIR)
CFLAGS += -Og -g
EXTRA_CFLAGS += -DDEBUG
LDLIBS += -l:librocket.a
endif

# Variables for output and intermediate artifacts
SOURCEDIR = src
TARGET = $(OBJDIR)/$(EXECUTABLE)
SOURCES = $(wildcard $(SOURCEDIR)/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES:$(SOURCEDIR)/%=$(OBJDIR)/%))
LIBRARIES = $(BUILDDIR)/lib/libSDL2.a $(BUILDDIR)/lib/librocket.a $(BUILDDIR)/include/stb_vorbis.c $(BUILDDIR)/include/data.c


# This rule is for linking the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) $(EXTRA_CFLAGS) $^ $(LDLIBS)
ifeq ($(DEBUG),0)
	$(STRIP) $(TARGET)
endif


# This rule is for compiling our C source files
$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(LIBRARIES)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<


# This rule is a check for having fetched git submodules
lib/SDL/configure:
	@echo Please run git submodule update --init first!
	@echo Check README.md for details.
	@exit 1


# This rule is for building SDL2
$(BUILDDIR)/lib/libSDL2.a: | lib/SDL/configure
	CC="$(CC)" scripts/build_sdl2.sh $(BUILDDIR)


# This rule is for building rocket libraries
$(BUILDDIR)/lib/librocket.a: | lib/SDL/configure
	$(MAKE) -C lib/rocket lib/librocket.a lib/librocket-player.a CFLAGS="-Os" CC="$(CC)"
	@mkdir -p $(BUILDDIR)/lib $(BUILDDIR)/include
	cp lib/rocket/lib/*.a $(BUILDDIR)/lib
	cp lib/rocket/lib/*.h $(BUILDDIR)/include


# This rule is for copying stb_vorbis.c to library include directory
$(BUILDDIR)/include/stb_vorbis.c: lib/stb/stb_vorbis.c
	@mkdir -p $(BUILDDIR)/include
	cp $^ $@


# This rule is for generating build/include/data.c
$(BUILDDIR)/include/data.c: $(wildcard data/*)
	@mkdir -p $(BUILDDIR)/include
	scripts/mkfs.sh $@

	
.PHONY: clean


clean:
	rm -rf $(BUILDDIR) $(RELEASEDIR)
	$(MAKE) -C lib/SDL $(MAKECMDGOALS)
	$(MAKE) -C lib/rocket $(MAKECMDGOALS)
