#!/usr/bin/env python

# Generates the content for an index.rst file
# from the content of a manual.tex file

import re
import sys

def print_header():
  sys.stdout.write(".. include:: index_header.rst\n")

def print_footer():
  sys.stdout.write("\n.. include:: index_footer.rst\n")

part_rx = re.compile(r"\\part{(.+)}")
include_rx = re.compile(r"\\include{(.+)}")
print_header()
for line in sys.stdin:
  m = part_rx.match(line)
  if m:
    sys.stdout.write("\n.. toctree::\n"
                     "   :maxdepth: 2\n"
                     "   :caption: ")
    sys.stdout.write(m.group(1))
    sys.stdout.write("\n\n")
    continue
  m = include_rx.match(line)
  if m:
    sys.stdout.write("   ")
    sys.stdout.write(m.group(1))
    sys.stdout.write("\n")
print_footer()
