#!/bin/bash
echo "erl -noshell -noinput +P 20000000 -sname benchmark -s $@ -s init stop" | ./exec.sh
