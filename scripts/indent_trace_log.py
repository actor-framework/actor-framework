#!/usr/bin/env python

# Indents a CAF log with trace verbosity. The script does *not* deal with a log
# with multiple threads.

# usage (read file): indent_trace_log.py FILENAME
#      (read stdin): indent_trace_log.py -

import argparse, sys, os, fileinput, re

def is_entry(line):
    return 'TRACE' in line and 'ENTRY' in line

def is_exit(line):
    return 'TRACE' in line and 'EXIT' in line

def print_indented(line, indent):
    if is_exit(line):
        indent = indent[:-2]
    sys.stdout.write(indent)
    sys.stdout.write(line)
    if is_entry(line):
        indent += "  "
    return indent

def read_lines(fp, ids):
    indent = ""
    if not ids or len(ids) == 0:
        for line in fp:
            indent = print_indented(line, indent)
    else:
        rx = re.compile('.+ (?:actor|ID = )([0-9]+) .+')
        for line in fp:
            rx_res = rx.match(line)
            if rx_res != None and rx_res.group(1) in ids:
                indent = print_indented(line, indent)

def main():
    parser = argparse.ArgumentParser(description='Add a new C++ class.')
    parser.add_argument('-i', dest='ids', action='append', help='only include actors with given ID(s)')
    parser.add_argument("log", help='path to the log file or "-" for reading from STDIN')
    args = parser.parse_args()
    filepath = args.log
    if filepath == '-':
        read_lines(fileinput.input(), args.ids)
    else:
        if not os.path.isfile(filepath):
            sys.exit()
        with open(filepath) as fp:
            read_lines(fp, args.ids)

if __name__ == "__main__":
    main()
