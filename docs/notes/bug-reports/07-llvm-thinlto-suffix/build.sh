#!/bin/sh
# usage: build.sh <dir> ; builds a.cpp b.cpp copied into <dir> with ThinLTO
set -e
src=$(dirname "$(readlink -f "$0")")
mkdir -p "$1" && cp "$src/a.cpp" "$src/b.cpp" "$1/" && cd "$1"
F="-O2 -flto=thin -ffile-prefix-map=$PWD/=  -fdebug-compilation-dir=."
clang++ $F -c "$PWD/a.cpp" -o a.o
clang++ $F -c "$PWD/b.cpp" -o b.o
clang++ -O2 -flto=thin -fuse-ld=lld a.o b.o -o prog
