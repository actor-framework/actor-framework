#!/bin/bash
#export JAVA_OPTS="-Xmx1024"
#echo "scala -cp /home/neverlord/akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar $@" | ./exec.sh
echo "/home/neverlord/scala/bin/scala -cp /home/neverlord/akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar $@" | ./exec.sh
