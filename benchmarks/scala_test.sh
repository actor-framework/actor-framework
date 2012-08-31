#!/bin/bash
#export JAVA_OPTS="-Xmx1024"
if [ ! -d $PWD/scala ] ; then
    echo "$PWD/scala is not a valid directory!"
    exit
fi

AKKA_LIBS=/$HOME/akka-2.0.3/lib/scala-library.jar
for JAR in /$HOME/akka-2.0.3/lib/akka/*.jar ; do
    AKKA_LIBS=$JAR:$AKKA_LIBS
done

arg0=org.libcppa.$1.Main
shift
JARS="$AKKA_LIBS":$PWD/scala/
echo "java -cp $JARS $arg0 $@" | ./exec.sh
