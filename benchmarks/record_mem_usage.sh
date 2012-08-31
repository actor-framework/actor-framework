#!/bin/bash

usage="\
Usage: $0 INTERVAL CMD

  example: $0 0.5 \"my_benchmark_program arg0 arg1 arg2 \"
"

if [ "$#" != "2"  ]; then
    echo "${usage}" 1>&2
    exit
fi

runtime=""
line=$(cat /proc/1/status | grep "^Vm" | awk 'BEGIN {printf "%-10s", "elapsed"} {printf "%-20s", $1} END {print ""}')

exec $2 &
pid=$!
interval=$1
dtime=$(date "+%s%N")
time0=$(echo "$dtime / 1000000" | bc -q)

fields=$(echo "$line" | wc -w)
while [ "$fields" -gt "1" ]; do
    echo "$line"
    if [ -n "$runtime" ]; then
        sleep $interval
        runtime=$(echo $runtime + $interval | bc -q)
    else
        runtime=0
    fi
    dtime=$(date "+%s%N")
    timeN=$(echo "( $dtime / 1000000 ) - $time0" | bc -q)
    timeN_sec=$(echo "scale=2; $timeN / 1000" | bc)
    line=$(cat /proc/${pid}/status 2>/dev/null | grep "^Vm" | awk -v rt=$timeN_sec 'BEGIN {printf "%-10s", rt} {printf "%-20s", ($2 " " $3)} END {print ""}')
    fields=$(echo "$line" | wc -w)
done

