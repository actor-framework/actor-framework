#!/usr/bin/env python3

# This script checks if any internal headers (placed inside `caf/internal`
# folder) are included inside public CAF library headers.

import re
import sys

pattern = re.compile(r'^\s*#\s*include\s*[<"]caf/internal/')

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <file> [<file> ...]", file=sys.stderr)
    sys.exit(2)

had_error = False

for path in sys.argv[1:]:
    try:
        with open(path, "r", encoding="utf-8") as f:
            for i, line in enumerate(f, start=1):
                if pattern.search(line):
                    print(f"{path}:{i}: forbidden include: {line.rstrip()}", file=sys.stderr)
                    had_error = True
    except OSError as e:
        print(f"{path}: cannot open file: {e}", file=sys.stderr)
        had_error = True

sys.exit(1 if had_error else 0)
