#!/bin/sh
[ -z "$1" ] && echo 'Usage: pack.sh executable' && exit 1
xz --format=lzma --lzma1=preset=9,lc=1,lp=0,pb=0 "$1"
cat "$(dirname $0)/start.sh" "$1.lzma" > "$1"
chmod +x "$1"
