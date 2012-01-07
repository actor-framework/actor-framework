#!/bin/bash
for i in *.erl; do
	echo "compile $i ..."
	erlc "$i"
done
echo done
