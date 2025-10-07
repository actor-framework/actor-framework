#!/bin/bash

# Custom compile timing wrapper script
# Usage: time_compile.sh <actual_compiler> [compiler_args...]

# Get the actual compiler and all arguments
ACTUAL_COMPILER="$1"
shift

# Extract the source file name from arguments
SOURCE_FILE=""
for arg in "$@"; do
    if [[ "$arg" == *.cpp ]] || [[ "$arg" == *.c ]] || [[ "$arg" == *.cc ]] || [[ "$arg" == *.cxx ]]; then
        SOURCE_FILE="$arg"
        break
    fi
done

# Start timing
START_TIME=$(date +%s.%N)

# Run the actual compiler
"$ACTUAL_COMPILER" "$@"
EXIT_CODE=$?

# End timing
END_TIME=$(date +%s.%N)

# Calculate duration
DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)

# Log the timing information
if [ -n "$SOURCE_FILE" ]; then
    echo "[COMPILE_TIME] $SOURCE_FILE: ${DURATION}s" >> /tmp/caf_compile_times.log
    echo "[COMPILE_TIME] $SOURCE_FILE: ${DURATION}s"
fi

# Return the original exit code
exit $EXIT_CODE

