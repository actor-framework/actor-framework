#!/usr/bin/env python

# This script demystifies C++ compiler output for CAF by
# replacing cryptic `typed_mpi<...>` templates with
# `replies_to<...>::with<...>` and `atom_constant<...>`
# with human-readable representation of the actual atom.

import sys

# decodes 6bit characters to ASCII
DECODING_TABLE = ' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'

# CAF type strings
ATOM_CONSTANT_SUFFIX = "caf::atom_constant<"

# `pos` points to first character after '<':
#   template_name<...>
#                 ^
# and returns the position of the closing '>'
def end_of_template(x, pos):
  open_templates = 1
  while open_templates > 0:
    if line[pos] == '<':
      open_templates += 1
    elif line[pos] == '>':
      open_templates -= 1
    pos += 1
  # exclude final '>'
  return pos - 1

def next_element(x, pos, last):
  # scan for ',' that isn't inside <...>
  while pos < last and x[pos] != ',':
    if x[pos] == '<':
      pos = end_of_template(x, pos + 1)
    else:
      pos += 1
  return pos

def atom_read(x):
  result = ""
  read_chars = ((x & 0xF000000000000000) >> 60) == 0xF
  mask = 0x0FC0000000000000
  bitshift = 54
  while bitshift >= 0:
    if read_chars:
      result += DECODING_TABLE[(x & mask) >> bitshift]
    elif ((x & mask) >> bitshift) == 0xF:
      read_chars = True
    bitshift -= 6
    mask = mask >> 6
  return result

def decompose_type_list(x, first, last):
  res = []
  i = first
  n = next_element(x, first, last)
  while i != last:
    res.append(x[i:n])
    # skip following ','
    i = min(n + 2, last)
    n = next_element(x, i, last)
  return res

def stringify(x):
  if x.startswith(ATOM_CONSTANT_SUFFIX):
    begin = len(ATOM_CONSTANT_SUFFIX)
    end = len(x) - 1
    res = "'"
    res += atom_read(int(x[begin:end]))
    res += "'"
    return res
  return x

def stringify_list(xs):
  res = ""
  for index in range(len(xs)):
    if index > 0:
      res += ", "
    res += stringify(xs[index].strip(' '))
  return res


def decompose_typed_actor(x, first, last):
  needle = "caf::type_list<"
  # first type list -> input types
  j = x.find(needle, first) + len(needle)
  k = end_of_template(x, j)
  inputs = decompose_type_list(x, j, k)
  # second type list -> outputs
  j = x.find(needle, k) + len(needle)
  k = end_of_template(x, j)
  outputs = decompose_type_list(x, j, k)
  # replace all 'caf::atom_constant<...>' entries in inputs
  res = "replies_to<"
  res += stringify_list(inputs)
  res += ">::with<"
  res += stringify_list(outputs)
  res += ">"
  return res


for line in sys.stdin:
  # replace "std::__1" with "std::" (Clang libc++)
  line = line.replace("std::__1", "std::")
  needle = "caf::typed_mpi<"
  idx = line.find(needle)
  while idx != -1:
    # find end of typed_actor<...>
    first = idx + len(needle)
    last = end_of_template(line, first)
    updated = decompose_typed_actor(line, first, last)
    prefix = line[:idx]
    suffix = line[last:]
    line = prefix + updated + suffix
    idx = line.find(needle, idx + len(updated))
  sys.stdout.write(line.replace("caf::", ""))

