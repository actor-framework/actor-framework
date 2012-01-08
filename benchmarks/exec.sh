#!/bin/bash
read -r cmd
export JAVA_OPTS="-Xmx4096M"
/usr/bin/time -p -f "%e" $cmd 2>&1 #| grep "^real" | grep -o -P "[0-9]*(\.[0-9]*)?"
