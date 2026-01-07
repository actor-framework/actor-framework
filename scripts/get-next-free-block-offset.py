#!/usr/bin/env python
# get-next-free-block-offset.py
#
# Scans *.test.cpp files recursively for CAF_BEGIN_TYPE_ID_BLOCK(...) entries,
# validates and extracts (OFFSET, SIZE), checks for overlaps, and prints the
# next free offset (max(OFFSET + SIZE)) on success.

from __future__ import annotations

import os
import re
from dataclasses import dataclass
from typing import List, Optional, Tuple


MACRO_NAME = "CAF_BEGIN_TYPE_ID_BLOCK"

# Second-parameter format:
#   caf::first_custom_type_id
#   caf::first_custom_type_id + 123
SECOND_PARAM_RE = re.compile(
    r"^\s*caf::first_custom_type_id\s*(?:\+\s*(\d+)\s*)?$"
)

# Third parameter (SIZE) must be a plain integer (allow whitespace).
SIZE_RE = re.compile(r"^\s*(\d+)\s*$")


@dataclass(frozen=True)
class Block:
    filename: str
    line: int
    offset: int
    size: int

    @property
    def start(self) -> int:
        return self.offset

    @property
    def end(self) -> int:
        return self.offset + self.size


def iter_test_cpp_files(root: str) -> List[str]:
    out: List[str] = []
    for dirpath, _, filenames in os.walk(root):
        for fn in filenames:
            if fn.endswith(".test.cpp"):
                out.append(os.path.join(dirpath, fn))
    return out


def line_number_at(text: str, idx: int) -> int:
    # 1-based
    return text.count("\n", 0, idx) + 1


def extract_macro_calls(text: str, filename: str) -> List[Tuple[str, int]]:
    """
    Returns list of tuples: (argument_string_inside_parentheses, start_line_number).
    Handles macro calls spanning multiple lines by balancing parentheses.
    """
    calls: List[Tuple[str, int]] = []
    search_from = 0

    while True:
        pos = text.find(MACRO_NAME, search_from)
        if pos == -1:
            break

        # Find the opening '(' after the macro name.
        i = pos + len(MACRO_NAME)
        n = len(text)
        while i < n and text[i].isspace():
            i += 1
        if i >= n or text[i] != "(":
            search_from = pos + 1
            continue

        start_line = line_number_at(text, pos)

        # Consume until matching ')'
        i += 1  # move past '('
        depth = 1
        arg_start = i
        while i < n and depth > 0:
            ch = text[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1

        if depth != 0:
            raise RuntimeError(
                f"{filename}:{start_line}: unterminated {MACRO_NAME}(...) call"
            )

        arg_str = text[arg_start:i]
        calls.append((arg_str, start_line))
        search_from = i + 1

    return calls


def split_top_level_args(arg_str: str) -> List[str]:
    """
    Splits by commas at top level (paren depth 0) within the argument string.
    """
    args: List[str] = []
    buf: List[str] = []
    depth = 0

    for ch in arg_str:
        if ch == "(":
            depth += 1
            buf.append(ch)
        elif ch == ")":
            depth = max(0, depth - 1)
            buf.append(ch)
        elif ch == "," and depth == 0:
            args.append("".join(buf).strip())
            buf = []
        else:
            buf.append(ch)

    tail = "".join(buf).strip()
    if tail:
        args.append(tail)

    return args


def parse_block_from_args(args: List[str], filename: str, line: int) -> Optional[Block]:
    if len(args) < 3:
        # Not enough arguments: raise an error since the macro pattern was successfully matched.
        raise ValueError(
            f"{filename}:{line}: {MACRO_NAME} expects at least 3 arguments, got {len(args)}"
        )

    # args[0] is the name, not relevant.
    second = args[1]
    third = args[2]

    m = SECOND_PARAM_RE.match(second)
    if not m:
        raise ValueError(
            f"{filename}:{line}: invalid 2nd parameter: {second!r}. "
            "Expected 'caf::first_custom_type_id' optionally '+ NUM'."
        )
    offset = int(m.group(1) or "0")

    m2 = SIZE_RE.match(third)
    if not m2:
        raise ValueError(
            f"{filename}:{line}: invalid 3rd parameter (SIZE): {third!r}. "
            "Expected a plain integer."
        )
    size = int(m2.group(1))
    if size <= 0:
        raise ValueError(
            f"{filename}:{line}: invalid 3rd parameter (SIZE): {third!r}. "
            "SIZE must be a positive integer greater than zero."
        )

    return Block(filename=filename, line=line, offset=offset, size=size)


def check_no_overlap(blocks: List[Block]) -> None:
    blocks_sorted = sorted(blocks, key=lambda b: (b.start, b.end))
    prev: Optional[Block] = None
    for b in blocks_sorted:
        if prev is not None and b.start < prev.end:
            raise RuntimeError(
                "Overlapping CAF_BEGIN_TYPE_ID_BLOCK ranges:\n"
                f"  - {prev.filename}:{prev.line}: offset={prev.offset}, size={prev.size} "
                f"(range [{prev.start}, {prev.end}))\n"
                f"  - {b.filename}:{b.line}: offset={b.offset}, size={b.size} "
                f"(range [{b.start}, {b.end}))"
            )
        prev = b


def main() -> int:
    root = os.getcwd()
    files = iter_test_cpp_files(root)

    blocks: List[Block] = []

    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError as e:
            raise RuntimeError(f"Failed to read {path}: {e}") from e

        for arg_str, start_line in extract_macro_calls(text, path):
            args = split_top_level_args(arg_str)
            blk = parse_block_from_args(args, path, start_line)
            if blk is not None:
                blocks.append(blk)

    if blocks:
        check_no_overlap(blocks)
        next_free = max(b.end for b in blocks)
    else:
        next_free = 0

    print(next_free)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as e:
        raise SystemExit(f"ERROR: {e}")
