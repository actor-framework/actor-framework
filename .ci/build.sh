#!/bin/sh
set -x
set -e

# This script expects the CAF source tree under 'sources', builds in 'build' and
# installs to 'bundle'. Additionally, the script expects the CMake pre-load
# script 'cmake-init.txt' in the current working directorys.
BaseDir="$PWD"
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

LeakSanCheck="
#include <cstdlib>
int main() {
  int* ptr = new int(EXIT_SUCCESS);
  return *ptr;
}
"

# Check that LeakSanitizer works if configured for this build.
if [ "${CAF_CHECK_LEAKS}" == "ON" ]; then
  mkdir -p "$BaseDir/LeakCheck"
  cd "$BaseDir/LeakCheck"
  echo "${LeakSanCheck}" > check.cpp
  c++ check.cpp -o check -fsanitize=address -fno-omit-frame-pointer
  out=`./check 2>&1 | grep -o LeakSanitizer`
  if [ "$out" != "LeakSanitizer" ]; then
    echo "unable to detected memory leaks on this platform!"
    return 1
  fi
  cd "$BaseDir"
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
$CTestCommand --output-on-failure
