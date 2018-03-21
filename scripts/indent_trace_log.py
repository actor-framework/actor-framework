#!/usr/bin/env python

# Indents a CAF log with trace verbosity. The script does *not* deal with a log
# with multiple threads.

# usage (read file): indent_trace_log.py FILENAME
#      (read stdin): indent_trace_log.py -

import sys
import os
import fileinput

def read_lines(fp):
    indent = ""
    for line in fp:
        if 'TRACE' in line and 'EXIT' in line:
            indent = indent[:-2]
        sys.stdout.write(indent)
        sys.stdout.write(line)
        if 'TRACE' in line and 'ENTRY' in line:
            indent += "  "

def main():
    filepath = sys.argv[1]
    if filepath == '-':
        read_lines(fileinput.input())
    else:
        if not os.path.isfile(filepath):
            sys.exit()
        with open(filepath) as fp:
            read_lines(fp)

if __name__ == "__main__":
    main()
