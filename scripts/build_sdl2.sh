#!/bin/sh
set -e
[ -z $CC ] && echo Set CC && exit 1
[ -z $BUILDDIR ] && echo Set BUILDDIR && exit 1
[ -z $MAKE ] && echo Set MAKE && exit 1

PREFIX="$(realpath $BUILDDIR)"
cd lib/SDL
./configure \
--prefix="$PREFIX" \
--enable-shared=no \
--enable-static=yes \
--enable-video=yes \
--enable-video-x11=yes \
--enable-video-wayland=yes \
--enable-video-kmsdrm=yes \
--enable-video-opengl=yes \
--enable-video-vulkan=no \
--enable-audio=yes \
--enable-alsa=yes \
--enable-pipewire=yes \
--enable-pulseaudio=no \
--enable-jack=no \
--enable-arts=no \
--enable-arts=no \
--enable-esd=no \
--enable-nas=no \
--enable-sndio=no \
--enable-render=no \
--enable-events=yes \
--enable-libudev=yes \
--enable-dbus=no \
--enable-ime=no \
--enable-fcitx=no \
--enable-joystick=no \
--enable-joystick-virtual=no \
--enable-haptic=no \
--enable-hidapi=no \
--enable-power=no \
--enable-filesystem=no \
--enable-timers=yes \
--enable-file=no \
--enable-misc=no \
--enable-locale=no \
--enable-sdl2-config=no \
CC=$CC CFLAGS="-Os"
$MAKE
$MAKE install
