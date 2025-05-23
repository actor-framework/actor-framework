#!/bin/sh
WorkingDir="$PWD"

UsageString="
     $0 build CMAKE_INIT_FILE SOURCE_DIR BUILD_DIR
  OR $0 test BUILD_DIR
  OR $0 test BUILD_DIR EXCLUDES
  OR $0 assert WHAT
"

usage() {
  echo "$UsageString"
  exit 1
}

isRelative() {
  case $1 in
    /*) echo "no" ;;
    *) echo "yes" ;;
  esac
}

makeAbsolute() {
  if [ `isRelative $1` = "yes" ]; then
    echo "$WorkingDir/$1"
  else
    echo "$1"
  fi
}

set -e

if [ $# = 4 ]; then
  if [ "$1" = 'build' ] && [ -f "$2" ] && [ -d "$3" ]; then
    Mode='build'
    InitFile=`makeAbsolute $2`
    SourceDir=`makeAbsolute $3`
    BuildDir=`makeAbsolute $4`
    mkdir -p "$BuildDir"
  else
    usage
  fi
elif [ $# = 2 ]; then
  if [ "$1" = 'test' ] && [ -d "$2" ]; then
    Mode='test'
    BuildDir=`makeAbsolute $2`
    Excludes=""
  elif [ "$1" = 'assert' ]; then
    Mode='assert'
    What="$2"
  else
    usage
  fi
elif [ $# = 3 ]; then
  if [ "$1" = 'test' ] && [ -d "$2" ]; then
    Mode='test'
    BuildDir=`makeAbsolute $2`
    Excludes="$3"
  else
    usage
  fi
else
    usage
fi

set -x

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

# Pick up Cirrus environment variables.
if [ -z "$CAF_NUM_CORES" ]; then
  if [ ! -z "$CIRRUS_CPU" ]; then
    CAF_NUM_CORES=$CIRRUS_CPU
  fi
fi

runBuild() {
  cat "$InitFile"
  cd "$BuildDir"
  $CMakeCommand -C "$InitFile" "$SourceDir"
  if [ -z "$CAF_NUM_CORES" ]; then
    $CMakeCommand --build . --target install
  else
    $CMakeCommand --build . --target install -- -j $CAF_NUM_CORES
  fi
  cd "$WorkingDir"
}

runTest() {
  if [ -z "$Excludes" ]; then
    # Workaround for debian 11  and ubuntu 20.04 where --test-dir is not supported
    (cd "$BuildDir" && exec $CTestCommand --output-on-failure)
  else
    $CTestCommand --test-dir "$BuildDir" --output-on-failure -E "$Excludes"
  fi
}

runLeakSanitizerCheck() {
  LeakSanCheckStr="
    int main() {
      int* ptr = new int(0);
      return *ptr;
    }
  "
  echo "${LeakSanCheckStr}" > LeakSanCheck.cpp
  c++ LeakSanCheck.cpp -o LeakSanCheck -fsanitize=address -fno-omit-frame-pointer
  out=`./LeakSanCheck 2>&1 | grep -o 'detected memory leaks'`
  if [ -z "$out" ]; then
    echo "unable to detected memory leaks on this platform!"
    return 1
  fi
}

runUBSanitizerCheck() {
  UBSanCheckStr="
    int main(int argc, char**) {
      int k = 0x7fffffff;
      k += argc;
      return 0;
    }
  "
  echo "${UBSanCheckStr}" > UBSanCheck.cpp
  c++ UBSanCheck.cpp -o UBSanCheck -fsanitize=undefined -fno-omit-frame-pointer
  out=`./UBSanCheck 2>&1 | grep -o 'signed integer overflow'`
  if [ -z "$out" ]; then
    echo "unable to detected undefined behavior on this platform!"
    return 1
  fi
}

if [ "$Mode" = 'build' ]; then
  runBuild
elif [ "$Mode" = 'test' ]; then
  runTest
else
  case "$What" in
    LeakSanitizer)
      runLeakSanitizerCheck
      ;;
    UBSanitizer)
      runUBSanitizerCheck
      ;;
    *)
    echo "unrecognized tag: $What!"
    return 1
  esac
fi
