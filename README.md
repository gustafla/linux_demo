# Simple Linux demo in C

Welcome. This is a guide to get started creating native OpenGL demos on Linux.
The repository is structured so that `data/` contains binary files
(assets such as music), `src/` contains source code, `scripts/` contains scripts
that are required for building the demo and `lib/` contains third-party
libraries (git submodules).

The result is an executable which shows some GLSL shaders with music,
sync and post processing, and doesn't depend on any other files.
We've chosen to compile and statically link all libraries except for libc,
so that the released production depends on least possible stuff.

## First-time setup

Warning! This project fails to build if it is stored in a path with spaces.
For example `/home/user/My Projects/linux_demo` won't work. This is due to
fundamental limitations in SDL2's build process.

You will need a C compiler (`gcc` or `clang` recommended) and some library
headers. This repository does not use a fancy build system, opting to use just
GNU `make` for simplicity.

### Ubuntu and Debian

```
sudo apt install build-essential xxd
```

### Arch Linux

```
sudo pacman -S base-devel vim
```

Then we need to fetch some third-party libraries:
```
git submodule update --init
```

## Building

Just run

```
make -j $(nproc)
```

This will use all CPU cores you have available.

## Overview of libraries and code

We will use the industry-standard SDL2 library as a base for audio output,
keyboard (quit-button), windowing and OpenGL context creation.

OpenGL along with your graphics card drivers will do the heavy lifting for
rendering your demo.

stb_vorbis is a single C source file audio codec library for OGG Vorbis,
which we use to avoid having to release a huge uncompressed audio file.

And finally, rocket is a sync tracker along with a library to edit
your demo's synchronization with music without having to change code and
recompile.

The `src/` directory contains TODO lines of commented C code, which you can
read starting from any file. Start from `main.c` for example.
