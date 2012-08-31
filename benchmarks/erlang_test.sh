#!/bin/bash
echo "erl -noshell -noinput +P 20000000 -setcookie abc123 -sname benchmark -pa erlang -s $@ -s init stop" | ./exec.sh
