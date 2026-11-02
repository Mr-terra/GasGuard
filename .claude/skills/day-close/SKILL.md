---
name: day-close
description: Hand over and close a GasGuard plan day after the author validates it - rebase on main, fast-forward merge, run the pre-push checks, push the day branch then main, and confirm CI. Use only after the author has explicitly validated the day.
---

# Close a plan day

Never run this skill before the author has validated the day in writing.
Validation is required by CLAUDE.md ("Push rights"); the review in
`review-day` must have been answered first.

## Preconditions (stop if any fails)
1. `git branch --show-current` is `day-N` and the working tree is clean
   (`git status --short` is empty).
2. The author's validation is in this conversation. If not, ask and stop.
3. `make check` is green on the day branch (run it now, read the tail).
4. `PLAN.md` items of the day are ticked; `README.md`, `docs/` and
   `docs/traceability.md` are up to date (CLAUDE.md "Documentation upkeep").

## Steps
1. Run the pre-push checks, in this order, and fix anything they flag in the
   same change set before continuing:
   - `trace-check` (regenerates `docs/traceability.md`)
   - `explain-module` for each module added during the day, if not done
   - `misra-review` only if the day's last commit did not already include it
2. Rebase on `main`: `git fetch origin` then `git rebase origin/main` (or
   `git rebase main` if no remote). If conflicts appear, stop and report
   them; never resolve them silently.
3. Re-run `make check` after the rebase. A red result stops the skill.
4. Merge on `main` fast-forward only:
   `git checkout main && git merge --ff-only day-N`.
   If the merge is not fast-forward, stop: the rebase was incomplete.
5. Push, in this order, and only these commands:
   `git push origin day-N` then `git push origin main`.
   Never `--force`, `-f`, or a remote branch deletion.
6. Check CI on `main`: `gh run list --branch main --limit 1`, then
   `gh run watch <id>` if it is still running. Report the result.
7. Report: the merge commit hash on `main`, the `make check` result, the CI
   status. If CI failed, fix it on a new branch `day-N-fix-<topic>`; never
   rewrite `main`.

## Rules
- Do not merge or push one step ahead of the checks.
- Do not start the next day's branch in this skill; that is a separate step.
- The commit message must not mention the company or the author's e-mail
  (CLAUDE.md "Commit style").

## Gotchas
- Run `make check` in WSL Ubuntu. The Git Bash shell has MinGW gcc, which
  builds binaries that cannot run the tests ("Exec format error").
- The CI on `main` runs Ubuntu 22.04. A green WSL run is strong evidence, but
  read the CI result: it is the only check that runs from a clean checkout.
- A pipeline such as `make check; echo $?` reports the last command's status,
  not make's. Use `if make check; then ... fi`.
