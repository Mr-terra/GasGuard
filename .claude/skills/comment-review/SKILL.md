---
name: comment-review
description: Check GasGuard C comments for tautology, meaning comments that only repeat what the name or the code already says, and list each one with a fix (rewrite, shorten or delete). Covers "How it works" blocks, Doxygen headers and inline comments. Use when the author asks about comment quality, before a commit that adds comments, or after a review of a module.
---

# Comment review (no tautology)

## Inputs
Default scope: **every comment in the project**, with no file left out. That means:
- C sources and headers: `*.c`, `*.h` in `app/`, `services/`, `hal/`, `drivers/`, `common/`, `tests/`, and `main.c`, including `third_party/` only when it is project-owned (Unity is vendored and is out of scope).
- Scripts and config: `tools/*.ps1`, `tools/*.py`, `tools/*.awk`, `.github/workflows/*.yml`, the `Makefile` (`#` lines), and `.claude/` hook or skill files that contain comments.
- Doxygen blocks, `@brief` lines, inline `//` and `/* */` comments, and `How it works` blocks.

A file list, a directory, or "the current diff" (`git diff --staged`, else `git diff`) narrows the scope when the author asks for it.

## Enumerating the comments (no sampling)
1. List the files in scope with `Glob`, one pattern per type, and count them.
2. For each file, `Grep` for comment markers (`//`, `/*`, `*`, and `#` in scripts and Makefiles) with `-n` and `output_mode: content`. Read the file with `Read` when the surrounding code is needed to judge a comment.
3. Work by directory, in batches of roughly 300 comment lines. Write the verdicts for a batch before starting the next one. Report the number of comments per batch, so that the total can be checked against the grep count.
4. A comment is reviewed only if it has been read. A grep count that is not matched by a verdict row is a gap, and the skill must say so.

## Duplicates across header and source
A `.c` definition whose `@brief` repeats the `.h` contract word for word is a
DELETE (remove the `@brief` line; keep `@param`, `@return`, `@req`, which R16
requires). A `.c` note that adds implementation detail (a register, a limit, an
ordering rule) is KEEP.

## What counts as a tautology
A comment is tautological when a reader gets nothing from it beyond the
identifier, the code on the next line, or the test name. Typical cases:
- Restates the function or test name: `/* Start a new stuck window. */` above `stuck_window_start()`.
- Narrates the next line: `/* Increment the counter. */` above `count++;`.
- Restates a computed value already visible in the code: `/* Clear level is 80. */` above `clear = al1 - hyst;` where `hyst` is 20 and `al1` is 100 (the number, not the reason).
- A "How it works" block that only describes the steps the code shows, with no reason, constraint or pitfall.
- A `@brief` that repeats the name with no extra information: `@brief Get the head.` on `eventlog_head()`.

## What is NOT a tautology (keep)
- The reason for a choice: why, not what (`first conversion after a channel change is inaccurate`).
- A hardware or datasheet constraint, a unit, a range, an ordering rule (`set levels before directions: no glitch`).
- A safety rationale that links to an FMEA row or a REQ.
- A non-obvious consequence (`returns GG_ERR_TIMEOUT instead of hanging`).
- Doxygen `@param`, `@return`, `@req` tags: required by R16, never flagged for form.
- Test comments that state the scenario's intent or the expected value when the test name does not.

## Target: minimum information, still readable
Each comment keeps only what a reader cannot get from the name or the code, and
must stay easy to read without opening the source:
- **Minimum**: one idea per comment. Remove any word that repeats the name, the
  code or a neighbouring comment. A "How it works" block keeps only the reasons,
  constraints and pitfalls.
- **Readable**: a complete sentence, in the language of the file (English in this
  project). No shorthand the reader must decode (`AL1 - hyst`, `ch 0`, `TX/RX ptr`):
  write the term in full once, then the value or reason. Cutting a comment to
  a fragment that needs the code to be understood is a fail, not a success.
- **Test**: after a proposed rewrite, read the comment alone, without the code.
  If the reader cannot say what the code does or why, the rewrite is too short:
  restore the missing reason, not the restatement.

## Steps
1. Read `docs/04-coding-standard.md` (R16, R18 and the teaching rule in CLAUDE.md).
2. Collect the comments: `Grep` for `//` and `/*` in the target files, with line numbers.
3. For each comment, read the code and the function or test name it belongs to, then decide: keep, rewrite, shorten or delete.
4. For a "How it works" block, check it line by line: keep only the sentences a reader could not get from the name or the code. The block stays within 5–10 lines (CLAUDE.md).
5. Do not change code, names or tags. Only comments are in scope.

## Output
A table, most tautological first:

| File:line | Comment (short quote) | Verdict | Proposed replacement |
|---|---|---|---|

Verdict: **DELETE** (pure repetition), **REWRITE** (keep the reason, drop the restatement), **SHORTEN** (useful but too long), **KEEP**. Each proposed replacement must pass the "read it alone" test above; add a column "Readable alone: yes/no" with the reason when it is no.

Then, for the whole scope: the count of comments read per directory against the grep count (they must match, or the gap is named), the count per verdict, the number of `How it works` blocks cut, and the number of `.c` `@brief` duplicates of a header.
Do not edit files unless the author asks; the table is the proposal.

## Gotchas
- A comment that repeats a name is only a tautology if it adds nothing; a comment that gives the unit or the range is useful even if it looks like a restatement.
- Doxygen form tags (`@param`, `@return`, `@req`) are required: do not flag them for being repetitive.
- Language: every proposed replacement and every note in the output is in English, the language of the project comments, whatever language the request was written in.
