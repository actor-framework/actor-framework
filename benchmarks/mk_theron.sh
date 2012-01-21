#!/bin/bash
IFLAGS="-I../../Theron/Include/"
LFLAGS="-L../../Theron/Lib -ltheron -lboost_thread"
FLAGS="-O2 -fno-strict-aliasing -DNDEBUG"
for i in theron_*.cpp ; do
    echo "compile $i"
    out_file=$(echo $i | sed 's/\(.*\)\..*/\1/')
    echo "out_file = $out_file"
    g++ -std=c++0x $i -o $out_file $FLAGS $LFLAGS $IFLAGS
done
