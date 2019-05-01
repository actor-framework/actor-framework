#!/usr/bin/env python

# Generates the content for an index.rst file
# from the content of a manual.tex file.
#
# Usage: make_index_rst.py <output-file> <input-file>

import re
import sys

part_rx = re.compile(r"\\part{(.+)}")
include_rx = re.compile(r"\\include{tex/(.+)}")
with open(sys.argv[1], 'w') as out_file:
  out_file.write(".. include:: index_header.rst\n")
  with open(sys.argv[2]) as tex_file:
    for line in tex_file:
      m = part_rx.match(line)
      if m:
        out_file.write("\n.. toctree::\n"
                         "   :maxdepth: 2\n"
                         "   :caption: ")
        out_file.write(m.group(1))
        out_file.write("\n\n")
        continue
      m = include_rx.match(line)
      if m:
        out_file.write("   ")
        out_file.write(m.group(1))
        out_file.write("\n")
  out_file.write("\n.. include:: index_footer.rst\n")
