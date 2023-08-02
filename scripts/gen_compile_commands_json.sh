#!/bin/sh
set -e
[ -z "$CC" ] && echo Set CC && exit 1
[ -z "$CFLAGS" ] && echo Set CFLAGS && exit 1

echo '['
for file in src/*.c; do
  echo '  {'
  echo '    "arguments": ['
  echo '      '\"$(which $CC)\",
  for flag in $CFLAGS; do
    echo '      '\"$flag\",
  done
  echo '    ],'
  echo '    '\"directory\":\ \"$PWD\",
  echo '    '\"file\":\ \"$PWD/$file\",
  echo '    '\"output\":\ \"$PWD/build/${file%.c}.o\"
  echo '  },'
done
echo ']'
