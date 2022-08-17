#!/bin/sh

CFLAGS="$(pkgconf --cflags sdl2)"
CFLAGS+="$(pkgconf --libs sdl2)"

echo $CFLAGS

gcc -O3 -W -Wall -Wextra -pedantic -o gaiseki gaiseki.c $CFLAGS -lm
