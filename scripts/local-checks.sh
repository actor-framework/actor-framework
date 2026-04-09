#!/bin/bash

# This script runs a series of validation checks on the project. It is intended
# to be run locally before submitting a pull request.
#
# The script validates that:
#   - the project builds (via CMake)
#   - the tests pass (via CTest)
#   - the code formatting is correct (via clang-format)
#   - no forbidden includes are found in the code base
#   - type-id block offsets in unit tests do not overlap

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$scriptDir/.." && pwd)" || exit 1

run_check() {
  local title="$1"
  echo ""
  echo "$title ..."
  $2 &> ".$2.log"
  if [ $? -eq 0 ]; then
    echo "OK"
    rm ".$2.log"
  else
    echo "FAILED"
    echo ""
    cat ".$2.log"
    rm ".$2.log"
    exit 1
  fi
}

# Prefer clang-format-19 if available. Otherwise use the system default.
if command -v clang-format-19 &>/dev/null; then
  clang_format_bin=clang-format-19
elif command -v clang-format &>/dev/null; then
  clang_format_bin=clang-format
else
  echo "clang-format not found" >&2
  exit 1
fi

cmake_check() {
  cmake --build build
}

ctest_check() {
  ctest --output-on-failure --test-dir build
}

clang_format_check() {
  find libcaf* -name '*.[ch]pp' | xargs "$clang_format_bin" --dry-run --Werror
}

forbidden_includes_check() {
  find libcaf_* -name '*hpp' -type f | grep -v 'caf/internal/' | xargs python3 scripts/forbidden-includes-check.py
}

type_id_block_offsets_check() {
  python3 scripts/get-next-free-block-offset.py 1>/dev/null
}

run_check "build project" cmake_check
run_check "run tests" ctest_check
run_check "check code formatting" clang_format_check
run_check "check for forbidden includes" forbidden_includes_check
run_check "check type-id block offsets" type_id_block_offsets_check

echo ""
if [ "$failed" -ne 0 ]; then
  echo "One or more checks failed." >&2
  exit 1
fi
echo "All checks passed."
