#!/bin/sh

bin_path=$VCL2LUA_PATH

if [ -z "$bin_path" ]; then
  bin_path=./vcl2lua51
fi

if [ "$#" -ne 1 ]; then
  echo "Usage: ./vcl2lua51-test.sh filename"
  exit -1
fi

cat $1 | $bin_path | awk '{ print $0 } END { print "test() print(\"DONE\")" }' | tee $.generated.lua | lua -
