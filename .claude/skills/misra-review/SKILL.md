---
name: misra-review
description: Review C source files or a diff against the GasGuard coding standard (MISRA C:2012 subset, rules R01–R20). Use before every commit, or when asked to review code quality.
---

# MISRA / coding-standard review

## Inputs
A file list, a directory, or "the current diff" (`git diff --staged`, else `git diff`).

## Steps
1. Read `docs/04-coding-standard.md` fully (rules and deviations register).
2. Run the tools and collect raw findings:
   - `make lint`
   - `grep -rnE '#include *<avr/' src/app src/services` (must be empty — R20)
   - `grep -rnE '\b(malloc|calloc|realloc|free|goto|printf|float|double)\b' src --include=*.c --exclude-dir=avr`
3. Read each file and check by hand what tools miss:
   - R05 single exit, R06 complete switches, R07 ignored returns, R08 implicit
     conversions, R12 non-boolean conditions, R15 `volatile` + atomic access,
     R16 Doxygen with `@req`, R17 argument checks, R18 magic numbers.
4. For each finding, check the deviations register: a registered deviation is
   not a finding.

## Output format
A table, most severe first:

| Severity | File:line | Rule | Finding | Fix |
|---|---|---|---|---|

Severity: **BLOCKER** (safety impact or R02/R03/R20), **MAJOR** (rule violation),
**MINOR** (style, docs).

Then: total per severity and a verdict **PASS** (no blocker/major) or **FAIL**.
Do not fix anything unless asked; propose the fix in the table.
