#!/bin/bash

VERSION=$(grep -oE "set\(LIBCPPA_VERSION_(MAJOR|MINOR|PATCH) [0-9]+" CMakeLists.txt | awk '{ if (NR > 1) printf "." ; printf $2 } END { printf "\n" }')
git tag -a "V$VERSION" -m "version $VERSION"
git push --tags
