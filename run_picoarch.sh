#!/bin/sh
export SDL_VIDEODRIVER="fbcon"
export SDL_FBACCEL=0
export SDL_FBDEV="/dev/fb0"
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
picoarch.bin $@
