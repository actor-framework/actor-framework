#!/bin/bash

# Alternative approach using grep to find functions returning caf::error
# This is less precise than clang-query but works without additional tools

echo "Finding functions that return caf::error using grep-based approach..."
echo "Note: This may include some false positives - use clang-query for precise results"
echo

# Pattern 1: Functions with explicit caf::error return type
echo "=== Functions with explicit caf::error return type ==="
grep -rn --include="*.cpp" --include="*.hpp" \
    -E "^\s*(virtual\s+)?(static\s+)?((caf::)?error|const\s+(caf::)?error(\s*&)?)\s+\w+\s*\(" . \
    | head -20

echo
echo "=== Functions with error return type (in caf namespace context) ==="
# Pattern 2: Functions returning "error" (likely in caf namespace)
grep -rn --include="*.cpp" --include="*.hpp" \
    -E "^\s*(virtual\s+)?(static\s+)?error\s+\w+\s*\(" . \
    | head -20

echo
echo "=== Method definitions returning error (class context) ==="
# Pattern 3: Method definitions that return error
grep -rn --include="*.cpp" --include="*.hpp" \
    -A 1 -B 1 \
    -E "error\s+\w+::\w+\s*\(" . \
    | head -20

echo
echo "=== Lambda/function expressions returning error ==="
# Pattern 4: Lambda expressions or function pointers returning error
grep -rn --include="*.cpp" --include="*.hpp" \
    -E "(->|return\s+type.*)\s*(caf::)?error" . \
    | head -10

echo
echo "=== Summary: Unique function signatures found ==="
# Collect and deduplicate function signatures
{
    grep -rh --include="*.cpp" --include="*.hpp" \
        -E "^\s*(virtual\s+)?(static\s+)?((caf::)?error|const\s+(caf::)?error(\s*&)?)\s+\w+\s*\(" . \
        | sed 's/^\s*//' | sort | uniq
    
    grep -rh --include="*.cpp" --include="*.hpp" \
        -E "error\s+\w+::\w+\s*\(" . \
        | sed 's/^\s*//' | sort | uniq
} | head -15

echo
echo "To get more precise results, install clang-query and use:"
echo "  ./run_caf_error_query.sh"