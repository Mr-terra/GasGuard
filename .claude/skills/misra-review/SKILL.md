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
   - `grep -rnE '#include *<avr/' app services` (must be empty — R20)
   - `grep -rnE '\b(malloc|calloc|realloc|free|goto|printf|float|double)\b' . --include=*.c --exclude-dir=avr`
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

## Gotchas
- A passing `make lint` can hide a failure: a wrapper script may swallow the
  exit code. Read the lizard and cppcheck lines, not only the status. (The
  `hal_i2c_write` complexity of 15 was hidden this way.)
- Static identifiers must be unique across the whole project (MISRA 5.9).
  Grep for the name before creating a `static` in a test file.
- cppcheck `unusedFunction` on Unity `setUp`/`tearDown` depends on the order
  files are analysed. Suppress per file only where it is reported, then rerun
  `make lint`.
- A new interrupt handler uses the DEV-07 deviation, not a new inline
  suppression. Add the handler to the register, do not copy the comment.
- `PORTD |= x` triggers `-Wconversion`. Use the `REG_SET` / `REG_CLEAR`
  macros with explicit casts.
