#!/bin/bash

for list in cached_stack blocking_cached_stack sutter_list blocking_sutter_list ; do
	echo "benchmarking $list..."
	rm -f "$list.dat"
	for i in 1000000 2000000 3000000 4000000 5000000 6000000 7000000 8000000 9000000 10000000 ; do
		_1=$(./queue_test $i 4 $list "\$TIME")
		_2=$(./queue_test $i 4 $list "\$TIME")
		_3=$(./queue_test $i 4 $list "\$TIME")
		_4=$(./queue_test $i 4 $list "\$TIME")
		_5=$(./queue_test $i 4 $list "\$TIME")
		MID=$(echo "scale=50;($_1+$_2+$_3+$_4+$_5)/5" | bc)
		MAX=$(echo "define max (x, y) { if (x < y) return (y); return (x); }; max(max(max(max($_1,$_2),$_3),$_4),$_5)" | bc)
		MIN=$(echo "define min (x, y) { if (x < y) return (x); return (y); }; min(min(min(min($_1,$_2),$_3),$_4),$_5)" | bc)
		POS_ERR=$(echo "$MAX-$MID" | bc)
		NEG_ERR=$(echo "$MID-$MIN" | bc)
		echo $[$i/1000000] $MID $POS_ERR $NEG_ERR >> "$list.dat"
	done
done

echo "done"

gnuplot gnuplot_file

