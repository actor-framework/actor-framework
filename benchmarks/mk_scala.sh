#!/bin/bash
for i in *.scala; do
	echo "scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar \"$i\""
	scalac -cp ../../akka-microkernel-1.2/lib/akka/akka-actor-1.2.jar "$i"
done
echo done
