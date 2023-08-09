# Common configuration variables
BUILDDIR = build
RELEASEDIR = release
EXECUTABLE = demo
CC = gcc
STRIP = strip --strip-all
EXTRA_CFLAGS = -MMD -std=c99 -Wall -Wextra -Wpedantic -Wno-unused-parameter -DGL_GLEXT_PROTOTYPES -I$(BUILDDIR)/include -L$(BUILDDIR)/lib
SOURCEDIR = src
SOURCES = $(wildcard $(SOURCEDIR)/*.c)
LIBRARIES = $(BUILDDIR)/lib/librocket.a $(BUILDDIR)/include/stb_vorbis.c
DEBUG ?= 1


# Add MinGW or Linux LDLIBS
ifeq ($(findstring mingw,$(CC)),mingw)
LDLIBS += $(shell /usr/x86_64-w64-mingw32/bin/sdl2-config --libs) -lglew32 -lopengl32
EXECUTABLE := $(EXECUTABLE).exe
else
LDLIBS += $(shell $(BUILDDIR)/bin/sdl2-config --static-libs) -lGL
LIBRARIES += $(BUILDDIR)/lib/libSDL2.a
endif


# Set debug and release build variables
ifeq ($(DEBUG),0)
OBJDIR = $(RELEASEDIR)
CFLAGS += -Os
EXTRA_CFLAGS += -DSYNC_PLAYER
LDLIBS += -lrocket-player
LIBRARIES += $(BUILDDIR)/include/data.c
else
OBJDIR = $(BUILDDIR)
CFLAGS += -Og -g
EXTRA_CFLAGS += -DDEBUG
LDLIBS += -lrocket
endif


# Variables for output and intermediate artifacts
TARGET = $(OBJDIR)/$(EXECUTABLE)
OBJS = $(SOURCES:%.c=$(OBJDIR)/%.o)
DEPS = $(OBJS:%.o=%.d)


# This rule is for linking the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(CFLAGS) $(EXTRA_CFLAGS) $^ $(LDFLAGS) $(LDLIBS)
ifeq ($(DEBUG),0)
	$(STRIP) $(TARGET)
else
	$(MAKE) compile_commands.json
	mkdir -p data/
endif
ifeq ($(findstring mingw,$(CC)),mingw)
	cp /usr/x86_64-w64-mingw32/bin/SDL2.dll $(OBJDIR)
	cp /usr/x86_64-w64-mingw32/bin/glew32.dll $(OBJDIR)
	cp /usr/x86_64-w64-mingw32/bin/libssp-0.dll $(OBJDIR)
endif


# Include compiler-generated header dependencies
-include $(DEPS)


# This rule is for compiling our C source files
$(OBJDIR)/%.o: %.c $(LIBRARIES)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<


# This rule is a check for having fetched git submodules
lib/SDL/configure:
	@echo Please run git submodule update --init first!
	@echo Check README.md for details.
	@exit 1


# This rule is for building SDL2
$(BUILDDIR)/lib/libSDL2.a: | lib/SDL/configure
	CC=$(CC) BUILDDIR=$(BUILDDIR) MAKE=$(MAKE) scripts/build_sdl2.sh


# This rule is for building rocket libraries
$(BUILDDIR)/lib/librocket.a: | lib/SDL/configure
	$(MAKE) -C lib/rocket lib/librocket.a lib/librocket-player.a CFLAGS="-Os" CC=$(CC)
	@mkdir -p $(BUILDDIR)/lib $(BUILDDIR)/include
	cp lib/rocket/lib/*.a $(BUILDDIR)/lib
	cp lib/rocket/lib/*.h $(BUILDDIR)/include


# This rule is for copying stb_vorbis.c to library include directory
$(BUILDDIR)/include/stb_vorbis.c: lib/stb/stb_vorbis.c
	@mkdir -p $(BUILDDIR)/include
	cp $^ $@


# This rule is for generating build/include/data.c
$(BUILDDIR)/include/data.c: $(wildcard shaders/*) $(wildcard data/*)
	@mkdir -p $(BUILDDIR)/include
	scripts/mkfs.sh shaders/ data/ > $@


# This generates a compile_commands.json file for clangd, clang-tidy etc. devtools
compile_commands.json: $(SOURCES)
	CC=$(CC) CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)" scripts/gen_compile_commands_json.sh $(SOURCEDIR) > $@


.PHONY: clean

clean:
	rm -rf $(BUILDDIR) $(RELEASEDIR) compile_commands.json
	rm -rf lib/rocket/lib/*.a
	rm -rf lib/rocket/lib/*.o
	$(MAKE) -C lib/SDL clean


.PHONY: run

# Compile and run the executable with 'make run'
run: $(TARGET)
	./$(TARGET)
