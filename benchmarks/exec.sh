#!/bin/bash
read -r cmd
export JAVA_OPTS="-Xmx5120M"
/usr/bin/time -p $cmd 2>&1 #| grep "^real" | grep -o -P "[0-9]*(\.[0-9]*)?"
