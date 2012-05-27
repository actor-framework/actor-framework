#!/bin/bash
echo "erl -noshell -noinput +P 20000000 -setcookie abc123 -sname benchmark -s $@ -s init stop" | ./exec.sh
