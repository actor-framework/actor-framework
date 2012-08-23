#!/bin/bash
#export JAVA_OPTS="-Xmx1024"
JARS="$AKKA_LIBS":/home/neverlord/akka-2.0.1/lib/scala-library.jar:./scala/
echo "java -cp $JARS $@" | ./exec.sh
