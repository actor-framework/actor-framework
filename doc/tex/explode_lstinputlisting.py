#!/usr/bin/env python

import sys
import re

def parse_range(astr):
    result = set()
    for part in astr.split(','):
        x = part.split('-')
        result.update(range(int(x[0]), int(x[-1]) + 1))
    return sorted(result)

def print_listing(line_range, fname):
    print('\\begin{lstlisting}')
    if not line_range:
        with open(fname, 'r') as fin:
            sys.stdout.write(fin.read())
    else:
        with open(fname) as mfile:
            for num, line in enumerate(mfile, 1):
                if num in line_range:
                    sys.stdout.write(line)
    print('\\end{lstlisting}')

def cppexample(line_range, fname):
    print_listing(line_range, '../../examples/{0}.cpp'.format(fname))

def iniexample(line_range, fname):
    print_listing(line_range, '../../examples/{0}.ini'.format(fname))

def sourcefile(line_range, fname):
    print_listing(line_range, '../../{0}'.format(fname))

rx = re.compile(r"\\(cppexample|iniexample|sourcefile)(?:\[(.+)\])?{(.+)}")

for line in sys.stdin:
    m = rx.match(line)
    if not m:
        sys.stdout.write(line)
    else:
        locals()[m.group(1)](parse_range(m.group(2)), m.group(3))
