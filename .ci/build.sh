#!/bin/sh
set -x
set -e

# This script expects the CAF source tree under 'sources', builds in 'build' and
# installs to 'bundle'. Additionally, the script expects the CMake pre-load
# script 'cmake-init.txt' in the current working directorys.
InitFile="$PWD/cmake-init.txt"
SourceDir="$PWD/sources"
BuildDir="$PWD/build"

cat "$InitFile"

# Prefer cmake3 over "regular" cmake (cmake == cmake2 on RHEL).
if command -v cmake3 >/dev/null 2>&1 ; then
  CMakeCommand="cmake3"
  CTestCommand="ctest3"
elif command -v cmake >/dev/null 2>&1 ; then
  CMakeCommand="cmake"
  CTestCommand="ctest"
else
  echo "No CMake found."
  exit 1
fi

# Make sure all directories exist, then enter build directory.
mkdir -p "$BuildDir"
cd "$BuildDir"

# Run CMake and CTest.
$CMakeCommand -C "$InitFile" "$SourceDir"
if [ -z "$CAF_NUM_CORES" ]; then
  $CMakeCommand --build . --target install
else
  $CMakeCommand --build . --target install -- -j $CAF_NUM_CORES
fi
$CTestCommand
