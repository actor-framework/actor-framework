#!/bin/bash
if [[ $# -eq 0 ]] ; then
    for i in *.scala; do
	    echo "scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar \"$i\""
	    scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar "$i"
    done
elif [[ $# -eq 1 ]] ; then
    echo "scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar \"$1.scala\""
    scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar "$1.scala"
fi
echo done
