#!/usr/bin/env python3

"""Verify UNIX line endings and trailing newlines for repository text files."""

from __future__ import annotations

import sys
from pathlib import Path

SUFFIXES = {".hpp", ".cpp", ".md", ".txt"}
SKIP_DIR_NAMES = {".git", "build"}


def iter_targets(root: Path) -> list[Path]:
    paths: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP_DIR_NAMES for part in path.parts):
            continue
        if path.suffix.lower() not in SUFFIXES:
            continue
        paths.append(path)
    return sorted(paths)


def main() -> int:
    if len(sys.argv) != 2:
        print(
            f"usage: {sys.argv[0]} <scan-dir>",
            file=sys.stderr,
        )
        return 2
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 2
    errors: list[str] = []
    for path in iter_targets(root):
        data = path.read_bytes()
        if b"\r" in data:
            errors.append(f"{path.relative_to(root)}: contains CR (use UNIX line endings)")
        if not data.endswith(b"\n"):
            errors.append(f"{path.relative_to(root)}: missing trailing newline (POSIX)")
    if errors:
        print("POSIX text file check failed:", file=sys.stderr)
        for msg in errors:
            print(f"  {msg}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
