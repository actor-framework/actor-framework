#!/bin/bash
for i in theron_*.cpp ; do
    echo compile $i ...
    g++ -std=c++0x -O3 $i -I../../Theron/Include/ -L../../Theron/Lib -ltheron -lboost_thread -o $(echo $i | grep -o -P "^[a-z_]+")
done
