#!/bin/bash

# Script to analyze CAF compile timing results
# Usage: analyze_compile_times.sh [log_file]

LOG_FILE="${1:-/tmp/caf_compile_times.log}"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: Log file '$LOG_FILE' not found."
    echo "Make sure to run a build with CAF_ENABLE_COMPILE_TIMING=ON first."
    exit 1
fi

echo "=== CAF Compile Time Analysis ==="
echo "Log file: $LOG_FILE"
echo ""

# Count total files compiled
TOTAL_FILES=$(wc -l < "$LOG_FILE")
echo "Total .cpp files compiled: $TOTAL_FILES"
echo ""

# Extract timing data and sort by duration
echo "=== Top 10 Slowest Compiling Files ==="
grep "\[COMPILE_TIME\]" "$LOG_FILE" | \
    sed 's/\[COMPILE_TIME\] //' | \
    sed 's/: / /' | \
    sort -k2 -nr | \
    head -10 | \
    awk '{printf "%-60s %8.3fs\n", $1, $2}'

echo ""

# Calculate statistics
echo "=== Compile Time Statistics ==="
TIMES=$(grep "\[COMPILE_TIME\]" "$LOG_FILE" | sed 's/.*: //' | sed 's/s$//')

if [ -n "$TIMES" ]; then
    TOTAL_TIME=$(echo "$TIMES" | awk '{sum += $1} END {print sum}')
    AVG_TIME=$(echo "$TIMES" | awk '{sum += $1; count++} END {print sum/count}')
    MIN_TIME=$(echo "$TIMES" | sort -n | head -1)
    MAX_TIME=$(echo "$TIMES" | sort -n | tail -1)

    echo "Total compile time: ${TOTAL_TIME}s"
    echo "Average compile time: ${AVG_TIME}s"
    echo "Minimum compile time: ${MIN_TIME}s"
    echo "Maximum compile time: ${MAX_TIME}s"
    echo ""

    # Files taking longer than average
    echo "=== Files Taking Longer Than Average (${AVG_TIME}s) ==="
    grep "\[COMPILE_TIME\]" "$LOG_FILE" | \
        sed 's/\[COMPILE_TIME\] //' | \
        sed 's/: / /' | \
        awk -v avg="$AVG_TIME" '$2 > avg {printf "%-60s %8.3fs\n", $1, $2}' | \
        sort -k2 -nr
fi

echo ""
echo "=== Summary by Directory ==="
grep "\[COMPILE_TIME\]" "$LOG_FILE" | \
    sed 's/\[COMPILE_TIME\] //' | \
    sed 's/: / /' | \
    awk '{
        # Extract directory from file path
        dir = $1
        gsub(/\/[^\/]*$/, "", dir)
        if (dir == "") dir = "."

        # Sum times by directory
        times[dir] += $2
        count[dir]++
    }
    END {
        for (d in times) {
            printf "%-40s %3d files, %8.3fs total, %6.3fs avg\n", d, count[d], times[d], times[d]/count[d]
        }
    }' | sort -k4 -nr

