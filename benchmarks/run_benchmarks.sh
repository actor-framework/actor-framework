#!/bin/bash

NumCores=$(cat /proc/cpuinfo | grep processor | wc -l)

if [ $NumCores -lt 10 ]; then
    NumCores="0$NumCores"
fi

declare -A impls
impls["cppa"]="event-based stacked"
impls["scala"]="akka threaded threadless"
impls["erlang"]="start"

declare -A benchmarks
benchmarks["cppa.mixed_case"]="mixed_case"
benchmarks["cppa.actor_creation"]="actor_creation"
benchmarks["cppa.mailbox_performance"]="mailbox_performance"
benchmarks["scala.mixed_case"]="MixedCase"
benchmarks["scala.actor_creation"]="ActorCreation"
benchmarks["scala.mailbox_performance"]="MailboxPerformance"
benchmarks["erlang.mixed_case"]="mixed_case"
benchmarks["erlang.actor_creation"]="actor_creation"
benchmarks["erlang.mailbox_performance"]="mailbox_performance"

declare -A bench_args

# 20 rings, 50 actors in each ring, initial token value = 10000, 5 repetitions
bench_args["mixed_case"]="20 50 10000 5"
# 2^19 actors
bench_args["actor_creation"]="19"
# 20 threads, 1,000,000 each = 20,000,000 messages
bench_args["mailbox_performance"]="20 1000000"

for Lang in "cppa" "scala" "erlang" ; do
    for Bench in "actor_creation" "mailbox_performance" "mixed_case" ; do
        Args=${bench_args[$Bench]}
        BenchName=${benchmarks["$Lang.$Bench"]}
        for Impl in ${impls[$Lang]}; do
            if [[ "$Lang.$Bench.$Impl" != "scala.actor_creation.threaded" ]] ; then
                echo "$Lang: $Impl $Bench ..." >&2
                FileName="$NumCores cores, $Lang $Impl $Bench.txt"
                exec 1>>"$FileName"
                for i in {1..5} ; do
                    ./${Lang}_test.sh $BenchName $Impl $Args
                done
            fi
        done
    done
done

exit

for Impl in "event-based" "stacked" ; do
    # actor creation performance
    FileName="cppa 2^19 $Impl actors, $NumCores cores.txt"
    echo "cppa performance: 2^19 $Impl actors..." >&2
    exec 1>"$FileName"
    for j in {1..10} ; do
        ./cppa_test.sh actor_creation $Impl 19
    done
    # mailbox performance
    FileName="cppa 20*1,000,000 messages $Impl, $NumCores cores.txt"
    echo "cppa mailbox performance: $Impl..." >&2
    exec 1>"$FileName"
    for j in {1..10} ; do
        ./cppa_test.sh mailbox_performance $Impl 20 1000000
    done
done

for Impl in "threaded" "akka" "threadless" ; do
    # actor creation performance
    FileName="scala 2^19 $Impl actors, $NumCores cores.txt"
    echo "scala performance: 2^19 $Impl actors" >&2
    exec 1>"$FileName"
    for j in {1..10} ; do
        ./scala_test.sh ActorCreation $Impl 19
    done
    # mailbox performance
    FileName="scala 20*1,000,000 messages $Impl, $NumCores cores.txt"
    echo "scala mailbox performance: $Impl..." >&2
    exec 1>"$FileName"
    for j in {1...10} ; do
        ./scala_test.sh MailboxPerformance $Impl 20 1000000
    done
done


