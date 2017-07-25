#!/bin/sh

bin_path=$VCL2LUA51_PATH

if [ -z "$bin_path" ]; then
  bin_path=./vcl2lua51
fi

if [ "$#" -ne 1 ]; then
  echo "Usage: ./vcl2lua51-syntax.sh filename"
  exit -1;
fi

cat $1 | $bin_path | source-highlight --src-lang=lua -f esc256
