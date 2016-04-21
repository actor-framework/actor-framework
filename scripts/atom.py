#!/usr/bin/env python

from __future__ import print_function
import sys

# decodes 6bit characters to ASCII
DECODING_TABLE = ' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'

def atom_read(x):
    result = ''
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

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('*** Usage: atom.py <atom_val>')
        sys.exit(-1)
    try:
        s = sys.argv[1]
        if s.startswith('0x'):
            val = int(s, 16)
        else:
            val = int(s)
        print(atom_read(val))
    except ValueError:
        print('Not a number:', sys.argv[1])
        print('*** Usage: atom.py <atom_val>')
        sys.exit(-2)
