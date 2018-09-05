#!/usr/bin/env python

import json
import sys
import re

from pandocfilters import toJSONFilter, Header, Str, RawBlock, RawInline, Image, Space

# This file fixes cross-references when building Sphinx documentation
# from the manual's .tex files.

# -- constants ----------------------------------------------------------------

singlefig_rx = re.compile(r"\\singlefig{(.+)}{(.+)}{(.+)}")

listing_rx = re.compile(r"\\(cppexample|iniexample|sourcefile)(?:\[(.+)\])?{(.+)}")

# -- factory functions --------------------------------------------------------

def make_rst_block(x):
    return RawBlock('rst', x)

def make_rst_ref(x):
    return RawInline('rst', ":ref:`" + x + "`")

def make_rst_image(img, caption, label):
    return make_rst_block('.. figure:: ' + img + '.png\n' +
                          '   :alt: ' + caption + '\n' +
                          '   :name: ' + label + '\n' +
                          '\n' +
                          '   ' + caption + '\n\n')

# -- code listing generation --------------------------------------------------

def parse_range(astr):
    if not astr:
        return set()
    result = set()
    for part in astr.split(','):
        x = part.split('-')
        result.update(range(int(x[0]), int(x[-1]) + 1))
    return sorted(result)

def make_rst_listing(line_range, fname, language):
    snippet = ''
    if not line_range:
        with open(fname, 'r') as fin:
            for line in fin:
                snippet += '   '
                snippet += line
    else:
        with open(fname) as mfile:
            for num, line in enumerate(mfile, 1):
                if num in line_range:
                    snippet += '   '
                    snippet += line
    return make_rst_block('.. code-block:: ' + language + '\n' +
                          '\n' +
                          snippet + '\n')

def cppexample(line_range, fname):
    return make_rst_listing(line_range, '../../examples/{0}.cpp'.format(fname), 'C++')

def iniexample(line_range, fname):
    return make_rst_listing(line_range, '../../examples/{0}.ini'.format(fname), 'ini')

def sourcefile(line_range, fname):
    return make_rst_listing(line_range, '../../{0}'.format(fname), 'C++')

# -- Pandoc callback ----------------------------------------------------------

def behead(key, value, format, meta):
    global singlefig_rx
    # pandoc does not emit labels before sections -> insert
    if key == 'Header':
        raw_lbl = value[1][0]
        if raw_lbl:
            lbl = '.. _' + raw_lbl + ':\n\n'
            value[1][0] = ''
            return [make_rst_block(lbl), Header(value[0], value[1], value[2])]
    # fix string parsing
    elif key == 'Str':
        if len(value) > 3:
            # pandoc generates [refname] as strings for \ref{refname} -> fix
            if value[0] == '[':
                return make_rst_ref(value[1:-1])
            elif value[1] == '[':
                return make_rst_ref(value[2:-1])
    # images don't have file endings in .tex -> add .png
    elif key == 'Image':
        return Image(value[0], value[1], [value[2][0] + ".png", value[2][1]])
    elif key == 'RawBlock' and value[0] == 'latex':
        # simply drop \clearpage
        if value[1] == '\\clearpage':
            return []
        # convert \singlefig to Image node
        m = singlefig_rx.match(value[1])
        if m:
            return make_rst_image(m.group(1), m.group(2), m.group(3))
        m = listing_rx.match(value[1])
        if m:
            return globals()[m.group(1)](parse_range(m.group(2)), m.group(3))
        sys.stderr.write("WARNING: unrecognized raw LaTeX" + str(value) + '\n')

if __name__ == "__main__":
  toJSONFilter(behead)

