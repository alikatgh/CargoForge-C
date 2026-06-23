<!-- Thanks for contributing to CargoForge-C! Keep PRs focused. -->

## What & why
Describe the change and the motivation. Link any related issue (`Closes #N`).

## How it was tested
- [ ] `make WERROR=1 test` passes (gcc and clang)
- [ ] `make sanitize` run is clean (AddressSanitizer + UBSan)
- [ ] `make analyze` (Clang static analyzer) is clean
- [ ] `./scripts/check.sh` passes end to end
- [ ] New behavior is covered by a test

## Checklist
- [ ] No new compiler warnings under `-Wall -Wextra -Werror`
- [ ] Stays dependency-free (pure C, no third-party libraries)
- [ ] Formatted with `make format` (`.clang-format`)
- [ ] Bug fix? Added a `docs/BUG_JOURNAL.md` entry in this same commit
- [ ] User-facing change? Noted under `## [Unreleased]` in `CHANGELOG.md`
