#!/usr/bin/env sh
# scripts/check.sh — the full local verification gate for CargoForge-C.
#
# Runs exactly what CI runs, in one command, from the project root:
#   1. build + unit tests with warnings promoted to errors (gcc and clang if both present)
#   2. the two example scenarios (smoke test the binary end to end)
#   3. AddressSanitizer + UBSan over the examples AND a known error path
#
# It is built to catch the bug classes this project has actually hit: a stale
# warning slipping in (-Werror), a regression in placement/analysis (unit tests),
# and memory/UB errors on the parser's failure paths (sanitizers) — the last is
# how the double-free was found. Exits non-zero on the first failure.
set -eu
cd "$(dirname "$0")/.."

run() { printf '\n=== %s ===\n' "$1"; shift; "$@"; }

# 1. Unit tests under every available compiler, warnings-as-errors.
for cc in gcc clang; do
    if command -v "$cc" >/dev/null 2>&1; then
        run "unit tests ($cc, -Werror)" make --silent clean
        make CC="$cc" WERROR=1 test
    fi
done

# 2. Example scenarios.
make --silent >/dev/null
run "sample scenario"    ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
run "realistic scenario" ./cargoforge examples/realistic_ship.cfg examples/realistic_cargo.txt

# CLI contract: --version prints the version and exits 0; --help exits 0; no args fails.
printf '\n=== CLI flags ===\n'
./cargoforge --version | grep -q "cargoforge " || { echo "FAIL: --version output" >&2; exit 1; }
./cargoforge --help >/dev/null               || { echo "FAIL: --help should exit 0" >&2; exit 1; }
if ./cargoforge >/dev/null 2>&1; then echo "FAIL: no-args should exit non-zero" >&2; exit 1; fi
echo "--version / --help / no-args OK"

# 3. Sanitizers over the examples and a deliberately-bad manifest (error path).
run "build sanitizer" make sanitize
# Note: LeakSanitizer (detect_leaks) is Linux-only; CI enables it there. Locally we
# rely on ASan's use-after-free / double-free / OOB checks, which work everywhere.
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
./cargoforge-san examples/sample_ship.cfg    examples/sample_cargo.txt    >/dev/null
./cargoforge-san examples/realistic_ship.cfg examples/realistic_cargo.txt >/dev/null
# Error path: a bad weight must error cleanly, not double-free.
printf 'GoodItem 10 5x5x5 general\nBadItem notanumber 5x5x5 general\n' > /tmp/cf_badweight.txt
if ./cargoforge-san examples/sample_ship.cfg /tmp/cf_badweight.txt >/dev/null 2>&1; then
    echo "FAIL: bad manifest should exit non-zero" >&2; exit 1
fi
rm -f /tmp/cf_badweight.txt

printf '\nAll checks passed.\n'
