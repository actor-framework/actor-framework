#!/bin/bash

# Script to find all function definitions that return caf::error using clang-query
# 
# Usage: ./run_caf_error_query.sh [files...]
# 
# If no files specified, it will search all .cpp and .hpp files in the project

set -e

QUERY_FILE="find_caf_error_functions.query"
BUILD_DIR="build"

# Check if clang-query is available
if ! command -v clang-query &> /dev/null; then
    echo "Error: clang-query is not installed. Please install clang-tools package."
    echo "On Ubuntu/Debian: sudo apt install clang-tools"
    echo "On macOS with brew: brew install llvm"
    exit 1
fi

# Check if query file exists
if [[ ! -f "$QUERY_FILE" ]]; then
    echo "Error: Query file $QUERY_FILE not found"
    exit 1
fi

# Determine files to analyze
if [[ $# -eq 0 ]]; then
    echo "Searching for .cpp and .hpp files..."
    FILES=$(find . -name "*.cpp" -o -name "*.hpp" | grep -v build | head -20)
else
    FILES="$@"
fi

echo "Running clang-query to find functions returning caf::error..."
echo "Files to analyze: $(echo $FILES | wc -w)"

# Basic include paths for CAF project
INCLUDES="-I libcaf_core/caf -I libcaf_net/caf -I libcaf_io/caf -I libcaf_openssl/caf -I libcaf_test/caf"

for file in $FILES; do
    echo "Analyzing: $file"
    clang-query "$file" -c "set output dump" -f "$QUERY_FILE" -- $INCLUDES -std=c++17 2>/dev/null || true
done

echo "Done."