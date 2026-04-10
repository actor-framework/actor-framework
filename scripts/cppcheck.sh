#!/bin/bash

# cppcheck.sh: Run cppcheck using the compile database and generate an HTML report.
#
# Usage: run from the repository root:
#
#   scripts/cppcheck.sh
#
# Passing no arguments will run cppcheck on everything.
#
# Optionally, the script takes any number of arguments, each of which is a
# directory to include in the scan. For example, to run cppcheck only on the
# core and net libraries:
#
#   scripts/cppcheck.sh libcaf_core libcaf_net
#
# Requires: cppcheck, cppcheck-htmlreport (Python), jq
# Output:   cppcheck-report.xml, cppcheck-report-html/ (HTML report)
#
# The compile database is read from build/compile_commands.json
# (or $CAF_BUILD_DIR/compile_commands.json if set).
#
# File-level suppressions are read from .cppcheck-suppressions at the repo root.

set -e

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repoRoot="$(cd "$scriptDir/.." && pwd)"

if [ -n "$CAF_BUILD_DIR" ]; then
  buildDir="$CAF_BUILD_DIR"
else
  buildDir="$repoRoot/build"
fi

compileCommands="$buildDir/compile_commands.json"
filteredCompileCommands="$buildDir/filtered_compile_commands.json"
reportXml="$repoRoot/cppcheck-report.xml"
reportHtmlDir="$repoRoot/cppcheck-report-html"

cd "$repoRoot"

if [ ! -f "$compileCommands" ]; then
  echo "Compile database not found: $compileCommands" >&2
  echo "Build the project with CMake and export compile commands." >&2
  exit 1
fi

for cmd in cppcheck cppcheck-htmlreport jq; do
  if ! command -v "$cmd" &>/dev/null; then
    echo "Required command not found: $cmd" >&2
    exit 1
  fi
done

# Number of parallel jobs from hardware concurrency (macOS + Linux).
if command -v nproc &>/dev/null; then
  jobs="$(nproc)"
else
  jobs="$(sysctl -n hw.ncpu 2>/dev/null || echo 1)"
fi

echo "[cppcheck] Building generated sources"
cmake --build "$buildDir" --target caf-code-gen

if [ $# -gt 0 ]; then
  echo "[cppcheck] Scanning specified directories: $@"
  filter=""
  for arg in "$@"; do
    filter="$filter or (.file | contains(\"$arg/\"))"
  done
  jq "[.[] | select(
        (
          (.file | (contains(\".test.hpp\") or contains(\".test.cpp\")) | not)
           and (.file | contains(\"/robot/\") | not)
        )
        and ( false $filter )
  )]" \
    "$compileCommands" >"$filteredCompileCommands"
else
  echo "[cppcheck] Scanning all directories"
  jq "[.[] | select(
        (.file | (contains(\".test.hpp\") or contains(\".test.cpp\")) | not)
         and (.file | contains(\"/robot/\") | not)
      )]" \
    "$compileCommands" >"$filteredCompileCommands"
fi

numFiles=$( jq '. | length' "$filteredCompileCommands" )

echo "[cppcheck] Found $numFiles files to scan"

suppressionsFile="$repoRoot/.cppcheck-suppressions"
echo "[cppcheck] Running cppcheck with compile database: $filteredCompileCommands"
set +e
cppcheck \
  -j "$jobs" \
  -D __cppcheck__ \
  --project="$filteredCompileCommands" \
  --xml \
  --xml-version=2 \
  --enable=performance,portability \
  --inline-suppr \
  --suppressions-list="$suppressionsFile" \
  --error-exitcode=7 \
  --check-level=exhaustive \
  2> "$reportXml"
cppcheck_exit=$?
set -e

if [ "$cppcheck_exit" -eq 0 ] || [ "$cppcheck_exit" -eq 7 ]; then
  echo "[cppcheck] Generating HTML report in $reportHtmlDir"
  cppcheck-htmlreport \
    --file="$reportXml" \
    --report-dir="$reportHtmlDir" \
    --source-dir="$repoRoot" \
    --title="Cppcheck report"

  echo "[cppcheck] Done. Open $reportHtmlDir/index.html to view the report."
fi

exit "$cppcheck_exit"
