#!/bin/bash
read -r cmd
export JAVA_OPTS="-Xmx4096M"
if [[ $(uname) == "Darwin" ]] ; then
    /usr/bin/time -p $cmd 2>&1
else
    /usr/bin/time -p -f "%e" $cmd 2>&1
fi
