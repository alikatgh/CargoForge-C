# Contributing to CargoForge-C

Thanks for your interest in CargoForge-C — an open-source, dependency-free C
simulator for maritime cargo logistics and ship stability. Contributions from C
developers, embedded folks, and logistics/naval-architecture people are all
welcome. The goal is a codebase that stays **modular, portable, and
dependency-free** while modelling real problems faithfully.

## Code of Conduct

Be respectful, collaborative, and constructive. Harassment or discrimination
won't be tolerated. Report issues to the maintainers via GitHub issues.

## Getting set up

```sh
git clone https://github.com/alikatgh/CargoForge-C.git
cd CargoForge-C
make WERROR=1 test     # build with warnings-as-errors and run the suite
```

You'll need a C99 compiler (gcc or clang) and `make`. There are no third-party
dependencies — that's a hard project constraint, please keep it that way.

## Before you open a pull request

Run the same gates CI runs. A PR that passes these locally will pass CI:

```sh
make WERROR=1 test     # 1. builds clean under -Werror, all tests green
make sanitize && \
  ./cargoforge-san examples/realistic_ship.cfg examples/realistic_cargo.txt
                       # 2. clean under AddressSanitizer + UBSan
make format            # 3. apply .clang-format (if installed)
```

Checklist:

- [ ] Builds with **both gcc and clang** under `-Werror` (no new warnings).
- [ ] `make test` passes; **new behavior comes with a test.**
- [ ] Memory- and UB-clean under `make sanitize`.
- [ ] Formatted with the project `.clang-format`.
- [ ] If you fixed a bug, add a 5-line entry to `docs/BUG_JOURNAL.md` **in the same
      commit as the fix** (newest first), and a "patterns to scan for" bullet if
      the lesson generalizes.
- [ ] Note user-facing changes under `## [Unreleased]` in `CHANGELOG.md`.

## Testing philosophy

Tests assert **invariants**, not frozen output. The placement heuristic's exact
coordinates will change as it improves — so test that *every item is placed, in
bounds, and non-overlapping*, and that *CG/GM match hand-computed values*, rather
than pinning magic numbers. See `tests/` for the pattern.

## Style

- C99, 4-space indent, attached braces, right-aligned pointers (`Cargo *c`).
  The `.clang-format` encodes this — run `make format`.
- Keep functions small and single-purpose. Prefer one source of truth over two
  functions that re-derive the same decision (see `find_placement` /
  `commit_placement` in `placement_2d.c`).
- Comment the *why* — invariants, units, lifetimes — not the obvious *what*.
- Units: weights are stored internally in **kilograms**, distances in **metres**.
  Config files take tonnes; the parser converts at the boundary.

## Commits & pull requests

- **Branches:** descriptive names — `feature/balanced-packing`, `fix/cg-units`.
- **Commits:** [Conventional Commits](https://www.conventionalcommits.org/) —
  e.g. `feat: add free-surface correction`, `fix: bound-check new shelf`,
  `docs: expand README`. Reference issues (`Closes #5`).
- **PRs:** base on `main`; describe *what*, *why*, and *how you tested it*. Add
  examples or tests where it makes sense. Expect review within about a week.

## Good first issues

- Add configurable bins/compartments instead of the fixed two-holds-plus-deck.
- Optimize longitudinal distribution within a bin (the transverse band is already
  centered; see the README roadmap).
- Add vertical stacking within a hold so the third dimension drives packing, not
  just the GM math.

Thanks for helping make CargoForge-C better.
