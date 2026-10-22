#!/bin/bash

# Runs clang-tidy over all translation units in the compilation database.
#
# The script reads the compilation database from the build directory by
# default. Use -p to select a different directory. Any additional arguments are
# passed through to run-clang-tidy. Before linting, the script creates the
# auto-generated CAF sources that are part of the compilation database.

set -euo pipefail

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repoDir="$(cd "$scriptDir/.." && pwd)"
buildDir=build
tidyArgs=()

usage() {
  cat <<EOF
Usage: $(basename "$0") [-p BUILD_DIR] [run-clang-tidy arguments...]

Runs clang-tidy over all files listed in BUILD_DIR/compile_commands.json.

Options:
  -p BUILD_DIR    Build directory containing compile_commands.json
                  (default: build)
  -h, --help      Show this help message

All other arguments are passed through to run-clang-tidy.
Set RUN_CLANG_TIDY to use a specific run-clang-tidy executable.
EOF
}

find_runner() {
  if [ -n "${RUN_CLANG_TIDY:-}" ]; then
    if command -v "$RUN_CLANG_TIDY" >/dev/null 2>&1; then
      command -v "$RUN_CLANG_TIDY"
      return 0
    fi
    return 1
  fi
  if command -v run-clang-tidy >/dev/null 2>&1; then
    command -v run-clang-tidy
    return 0
  fi
  if command -v run-clang-tidy.py >/dev/null 2>&1; then
    command -v run-clang-tidy.py
    return 0
  fi
  return 1
}

while [ $# -gt 0 ]; do
  case "$1" in
    -p|--build-dir)
      if [ $# -lt 2 ]; then
        echo "Missing argument for $1" >&2
        usage >&2
        exit 2
      fi
      buildDir="$2"
      shift 2
      ;;
    -p=*|--build-dir=*)
      buildDir="${1#*=}"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      tidyArgs+=("$@")
      break
      ;;
    *)
      tidyArgs+=("$1")
      shift
      ;;
  esac
done

cd "$repoDir"

if [ ! -f "$buildDir/compile_commands.json" ]; then
  echo "error: $buildDir/compile_commands.json not found" >&2
  echo "Configure the project first, e.g. by running './configure'." >&2
  exit 1
fi

if ! runner="$(find_runner)"; then
  echo "error: run-clang-tidy not found" >&2
  exit 1
fi

# The compilation database contains generated enum sources. Generate them
# without building the rest of the project.
if ! cmake --build "$buildDir" --target caf-code-gen; then
  echo "error: failed to generate sources required by clang-tidy" >&2
  exit 1
fi

exec "$runner" -p "$buildDir" "${tidyArgs[@]}"
