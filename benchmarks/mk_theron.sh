#!/bin/bash
IFLAGS="-I../../Theron/Include/"
LFLAGS="-L../../Theron/Lib -ltheron -lboost_thread"
FLAGS="-O3 -fno-strict-aliasing -DNDEBUG -DTHERON_MAX_ACTORS=530000"

if [[ $# -eq 0 ]] ; then
    for i in theron_*.cpp ; do
        out_file=$(echo $i | sed 's/\(.*\)\..*/\1/')
        echo "g++ -std=c++0x $i -o $out_file $FLAGS $LFLAGS $IFLAGS"
        g++ -std=c++0x $i -o $out_file $FLAGS $LFLAGS $IFLAGS
    done
elif [[ $# -eq 1 ]] ; then
    echo "g++ -std=c++0x theron_$1.cpp -o theron_$1 $FLAGS $LFLAGS $IFLAGS"
    g++ -std=c++0x theron_$1.cpp -o theron_$1 $FLAGS $LFLAGS $IFLAGS
fi

