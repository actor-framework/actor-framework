#!/bin/bash
echo "erl -noshell -noinput +P 20000000 -s $@ -s init stop" | ./exec.sh
