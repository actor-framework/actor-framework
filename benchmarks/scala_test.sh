#!/bin/bash
#export JAVA_OPTS="-Xmx1024"
echo "scala -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar $@" | ./exec.sh
