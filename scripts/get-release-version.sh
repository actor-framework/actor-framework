#!/bin/sh

grep "define CAF_VERSION" libcaf_core/caf/config.hpp | awk '{printf "%d.%d.%d", int($3 / 10000), int($3 / 100) % 100, $3 % 100}' > version.txt
git log --pretty=format:%h -n 1 > sha.txt
if test "$(cat branch.txt) = master" && git describe --tags --contains $(cat sha.txt) 1>tag.txt 2>/dev/null ; then
  cp tag.txt release.txt
else
  printf "%s" "$(cat version.txt)+exp.sha.$(cat sha.txt)" > release.txt
fi
