#!/bin/bash
if [[ $# -eq 0 ]] ; then
    for i in *.scala; do
	    echo "compile \"$i\""
	    scalac -unchecked -cp $AKKA_LIBS "$i"
    done
elif [[ $# -eq 1 ]] ; then
    echo "compile \"$1.scala\""
    scalac -unchecked -cp $AKKA_LIBS "$1.scala"
fi
echo done
