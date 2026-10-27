#!/usr/bin/env python3
"""Claude Code PostToolUse hook: block edits that break non-negotiable rules.

Reads the hook JSON on stdin, inspects the edited C file, and exits with
code 2 (feedback sent back to Claude) when a forbidden construct is found.
Fast regex checks only; the full analysis is `make lint`.
"""
import json
import re
import sys
from pathlib import Path

FORBIDDEN_ALL = [
    (r"\b(malloc|calloc|realloc|free)\s*\(", "R02: dynamic allocation is forbidden"),
    (r"\bgoto\b", "R04: goto is forbidden"),
    (r"\b(s?printf|scanf)\s*\(", "R13: printf/scanf family is forbidden"),
]
FORBIDDEN_LOGIC = [
    (r"#\s*include\s*<avr/", "R20: app/ and services/ must not include AVR headers"),
    (r"\b(float|double)\b", "R14: no floating point outside hal/avr/"),
]


def strip_comments(code: str) -> str:
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.S)
    return re.sub(r"//[^\n]*", "", code)


def main() -> int:
    try:
        event = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0
    path = (event.get("tool_input") or {}).get("file_path", "")
    if not path.endswith((".c", ".h")) or "/tests/" in path.replace("\\", "/"):
        return 0
    file = Path(path)
    if not file.is_file():
        return 0
    code = strip_comments(file.read_text(encoding="utf-8", errors="replace"))
    norm = path.replace("\\", "/")
    rules = list(FORBIDDEN_ALL)
    if "/src/app/" in norm or "/src/services/" in norm:
        rules += FORBIDDEN_LOGIC
    problems = []
    for pattern, message in rules:
        for match in re.finditer(pattern, code):
            line = code.count("\n", 0, match.start()) + 1
            problems.append(f"{path}:{line}: {message}")
    if problems:
        print("Coding-standard guard (docs/04-coding-standard.md):", file=sys.stderr)
        print("\n".join(problems), file=sys.stderr)
        print("Fix these before continuing; do not work around the hook.", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
