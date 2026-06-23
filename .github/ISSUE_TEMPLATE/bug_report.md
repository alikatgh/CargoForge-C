---
name: Bug report
about: Report incorrect behavior, a crash, or a wrong stability result
title: "[bug] "
labels: bug
---

**What happened**
A clear description of the bug.

**Steps to reproduce**
Command and input files, e.g.:

```sh
./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
```

If the inputs are custom, paste the ship `.cfg` and cargo manifest (or attach them).

**Expected vs. actual**
- Expected:
- Actual (paste the program output):

**Environment**
- OS / arch:
- Compiler and version (`gcc --version` / `clang --version`):
- `cargoforge --version`:

**Checked**
- [ ] Reproduces on a clean `make` build
- [ ] Ran `make sanitize` and pasted any ASan/UBSan trace above (if a crash/UB)
