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
    if len(ids) == 0:
        for line in fp:
            indent = print_indented(line, indent)
    else:
        rx = re.compile('.+ (?:actor|ID = )([0-9]+) .+')
        for line in fp:
            rx_res = rx.match(line)
            if rx_res != None and rx_res.group(1) in ids:
                indent = print_indented(line, indent)

def read_ids(ids_file):
    if ids_file and len(ids_file) > 0:
        if os.path.isfile(ids_file):
            with open(ids_file) as fp:
                return fp.read().splitlines()
    return []

def main():
    parser = argparse.ArgumentParser(description='Add a new C++ class.')
    parser.add_argument('-i', dest='ids_file', help='only include actors with IDs from file')
    parser.add_argument("log", help='path to the log file or "-" for reading from STDIN')
    args = parser.parse_args()
    filepath = args.log
    ids = read_ids(args.ids_file)
    if filepath == '-':
        read_lines(fileinput.input(), ids)
    else:
        if not os.path.isfile(filepath):
            sys.exit()
        with open(filepath) as fp:
            read_lines(fp, ids)

if __name__ == "__main__":
    main()
