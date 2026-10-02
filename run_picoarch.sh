#!/bin/sh
# Launcher for picoarch on Calculinux/PicoCalc.
#
# Mirrors /etc/profile.d/sdl2-defaults.sh so the app also works when
# started outside a login shell (e.g. over ssh or from a launcher):
# the custom SDL2 KMSDRM backend paints straight to the 320x320 SPI LCD
# dumb buffer and the evdev driver feeds (and grabs) the keyboard.

if [ -z "$XDG_RUNTIME_DIR" ]; then
    XDG_RUNTIME_DIR="/tmp/runtime-$(id -u)"
    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 0700 "$XDG_RUNTIME_DIR"
    export XDG_RUNTIME_DIR
fi

export SDL_VIDEODRIVER=kmsdrm
export SDL_RENDER_DRIVER=software
export SDL_AUDIODRIVER=alsa
# 2 = keyboard class
export SDL_EVDEV_DEVICES=2:/dev/input/event0

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$DIR/picoarch.bin" "$@"
