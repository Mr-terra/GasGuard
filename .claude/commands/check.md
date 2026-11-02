---
description: Run the full GasGuard quality gate (make check) in WSL Ubuntu and report the result
---

Run the full quality gate inside WSL Ubuntu. Do not use the Git Bash shell:
its MinGW gcc builds binaries that cannot run the tests.

```
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/arthu/Desktop/Projet_perso/GasGuard && if make check > /tmp/gg_check.log 2>&1; then echo "CHECK PASS"; else echo "CHECK FAIL"; fi; grep -n "make: \*\*\*" /tmp/gg_check.log | head -5; grep -E "^flash|Traceability" /tmp/gg_check.log'
```

Then report, in this order:
1. The verdict, PASS or FAIL, copied from the output.
2. On FAIL: the first `make: ***` line and the 20 lines of log before it,
   quoted as they are. Do not guess the cause.
3. On PASS: the flash usage and the traceability count from the log.

Do not fix anything in this command. A failure is handed to the author.
