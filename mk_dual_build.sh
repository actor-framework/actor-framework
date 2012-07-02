#!/bin/bash

if test "$#" != "2" ; then
    echo "usage: $0 [path_to_clang++] [path_to_g++]"
    exit
fi

echo "using clang: $1"
echo "using gcc: $2"

rm -rf clang_build
rm -r gcc_build

topDir=$PWD

mkdir clang_build
cd clang_build
cmake .. -DCMAKE_CXX_COMPILER="$1" -DCMAKE_CXX_FLAGS="-stdlib=libc++ -pedantic -Wall -Wextra -Werror" -DEXECUTABLE_OUTPUT_PATH="$topDir/clang_bin/" -DLIBRARY_OUTPUT_PATH="$topDir/clang_lib"
cd ..

mkdir gcc_build
cd gcc_build
cmake .. -DCMAKE_CXX_COMPILER="$2" -DCMAKE_CXX_FLAGS="-pedantic -Wall -Wextra -Werror" -DEXECUTABLE_OUTPUT_PATH="$topDir/gcc_bin/" -DLIBRARY_OUTPUT_PATH="$topDir/gcc_lib"
cd ..

exec > Makefile
printf "all:\n"
printf "\tmake -C gcc_build\n"
printf "\tmake -C clang_build\n"
printf "\n"
printf "clean:\n"
printf "\tmake clean -C gcc_build\n"
printf "\tmake clean -C clang_build\n"
printf "\n"
printf "test:\n"
printf "\texport DYLD_LIBRARY_PATH=$topDir/gcc_lib\n"
printf "\t./gcc_bin/unit_tests\n"
printf "\texport DYLD_LIBRARY_PATH=$topDir/clang_lib\n"
printf "\t./clang_bin/unit_tests\n"
