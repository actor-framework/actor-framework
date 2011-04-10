#!/bin/bash

NUM_THREADS=8
XUNIT=1000000

# $1 = list name
# $2 = num messages
function exec_queue_test()
{
	echo "./queue_test $2 $NUM_THREADS $1"
	# run test five times
	_1=$(./queue_test $2 $NUM_THREADS $1 "\$TIME")
	_2=$(./queue_test $2 $NUM_THREADS $1 "\$TIME")
	_3=$(./queue_test $2 $NUM_THREADS $1 "\$TIME")
	_4=$(./queue_test $2 $NUM_THREADS $1 "\$TIME")
	_5=$(./queue_test $2 $NUM_THREADS $1 "\$TIME")
	# get average runtime
	MID=$(echo "scale=50;($_1+$_2+$_3+$_4+$_5)/5" | bc)
	# get max. runtime
	MAX=$(echo "define max (x, y) { if (x < y) return (y); return (x); }; max(max(max(max($_1,$_2),$_3),$_4),$_5)" | bc)
	# get min. runtime
	MIN=$(echo "define min (x, y) { if (x < y) return (x); return (y); }; min(min(min(min($_1,$_2),$_3),$_4),$_5)" | bc)
	# error
	ERR=$(echo "scale=50; $MAX-$MIN" | bc)
	# X value for gnuplot
	XVAL=$(echo "scale=4; $2/$XUNIT" | bc)
	# print result to file
	echo "$XVAL $MID $ERR" >> "$1.dat"
}

# $1 = list name
# $2 = minimum num messages
# $3 = maximum num messages
# $4 = step
function test_run()
{
	#rm -f "$list.dat"
	i=$2
	while test "$i" -le "$3" ; do
	    exec_queue_test $1 $i
		XVAL=$(echo "scale=4; $i/$XUNIT" | bc)
		i=$[$i+$4]
	done
}

#for list in intrusive_sutter_list cached_stack lockfree_list ; do
for list in michael_scott_lockfree ; do
	echo "benchmarking $list ..."
	rm -f "$list.dat"
	test_run $list 1000000 10000000 9000000
	test_run $list 20000000 110000000 10000000
done

echo "done"

gnuplot -e "FILE='i7_${NUM_THREADS}threads.png';TITLE='2.66 GHz i7 (dual core) with $NUM_THREADS producer threads'" gnuplot_file

