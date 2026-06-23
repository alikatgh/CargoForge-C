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

# --json must emit valid, parseable JSON.
if command -v python3 >/dev/null 2>&1; then
    ./cargoforge --json examples/realistic_ship.cfg examples/realistic_cargo.txt \
        | python3 -m json.tool >/dev/null || { echo "FAIL: --json not valid JSON" >&2; exit 1; }
    echo "--json valid JSON OK"
else
    ./cargoforge --json examples/realistic_ship.cfg examples/realistic_cargo.txt \
        | grep -q '"summary"' || { echo "FAIL: --json missing summary" >&2; exit 1; }
    echo "--json OK (python3 unavailable; key-checked only)"
fi

# --strict: exit 0 on a good plan, non-zero on an overweight one.
./cargoforge --strict examples/realistic_ship.cfg examples/realistic_cargo.txt >/dev/null \
    || { echo "FAIL: --strict on a good plan should exit 0" >&2; exit 1; }
printf 'length_m=50\nwidth_m=10\nmax_weight_tonnes=100\nlightship_weight_tonnes=50\n' > /tmp/cf_tiny.cfg
printf 'Heavy 200 5x5x5 bulk\n' > /tmp/cf_heavy.txt
if ./cargoforge --strict /tmp/cf_tiny.cfg /tmp/cf_heavy.txt >/dev/null 2>&1; then
    echo "FAIL: --strict on an overweight plan should exit non-zero" >&2
    rm -f /tmp/cf_tiny.cfg /tmp/cf_heavy.txt; exit 1
fi
rm -f /tmp/cf_tiny.cfg /tmp/cf_heavy.txt
echo "--strict OK"

# Output modes: quiet/verbose and color control.
ESC=$(printf '\033')
S=examples/sample_ship.cfg; C=examples/sample_cargo.txt
./cargoforge --quiet   "$S" "$C" | grep -q "Load Summary" || { echo "FAIL: --quiet"   >&2; exit 1; }
./cargoforge --verbose "$S" "$C" | grep -q "of cargo"     || { echo "FAIL: --verbose" >&2; exit 1; }
./cargoforge --color=always "$S" "$C" | grep -q "$ESC"    || { echo "FAIL: --color=always emitted no ANSI" >&2; exit 1; }
if ./cargoforge --color=never "$S" "$C" | grep -q "$ESC"; then echo "FAIL: --color=never emitted ANSI" >&2; exit 1; fi
./cargoforge --diagram "$S" "$C" | grep -q "Stowage Plan" || { echo "FAIL: --diagram" >&2; exit 1; }
./cargoforge --csv "$S" "$C" | head -1 | grep -q "^id,x_m," || { echo "FAIL: --csv header" >&2; exit 1; }
./cargoforge --md  "$S" "$C" | grep -q "^# CargoForge Stowage Plan" || { echo "FAIL: --md" >&2; exit 1; }
./cargoforge --summary "$S" "$C" | grep -q "placed |"  || { echo "FAIL: --summary" >&2; exit 1; }
./cargoforge --table   "$S" "$C" | grep -q "┌"          || { echo "FAIL: --table" >&2; exit 1; }
./cargoforge --progress "$S" "$C" 2>&1 >/dev/null | grep -q "Optimizing" || { echo "FAIL: --progress" >&2; exit 1; }
echo "output modes OK"

# Input flexibility: --init round-trips through --show-config on stdin; stdin cargo; env override.
./cargoforge --init | ./cargoforge --show-config - | grep -q "^length_m=150" || { echo "FAIL: --init/--show-config" >&2; exit 1; }
cat "$C" | ./cargoforge "$S" - | grep -q "Placed /"                        || { echo "FAIL: stdin cargo" >&2; exit 1; }
CARGOFORGE_HOLDS=3 ./cargoforge --show-config "$S" | grep -q "^holds=3"     || { echo "FAIL: env override" >&2; exit 1; }
echo "input modes OK"

# Cargo attributes surface in the Cargo Notes section.
./cargoforge "$S" tests/fixtures/attrs_cargo.txt 2>/dev/null | grep -q "Reefer power demand" || { echo "FAIL: cargo notes" >&2; exit 1; }
echo "cargo notes OK"

# 3. Static analysis (Clang static analyzer), if clang is available.
if command -v clang >/dev/null 2>&1; then
    run "static analysis" make --silent analyze
fi

# 4. Sanitizers over the examples and a deliberately-bad manifest (error path).
run "build sanitizer" make sanitize
# Note: LeakSanitizer (detect_leaks) is Linux-only; CI enables it there. Locally we
# rely on ASan's use-after-free / double-free / OOB checks, which work everywhere.
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
./cargoforge-san examples/sample_ship.cfg    examples/sample_cargo.txt    >/dev/null
./cargoforge-san examples/realistic_ship.cfg examples/realistic_cargo.txt >/dev/null
# Over-long-line path (exercises the line-draining loop under ASan/UBSan).
./cargoforge-san examples/sample_ship.cfg tests/fixtures/longline_cargo.txt >/dev/null
# Configurable holds (exercises the dynamic bin alloc/free under ASan/UBSan).
./cargoforge-san tests/fixtures/holds4_ship.cfg examples/sample_cargo.txt >/dev/null
# Error path: a bad weight must error cleanly, not double-free.
printf 'GoodItem 10 5x5x5 general\nBadItem notanumber 5x5x5 general\n' > /tmp/cf_badweight.txt
if ./cargoforge-san examples/sample_ship.cfg /tmp/cf_badweight.txt >/dev/null 2>&1; then
    echo "FAIL: bad manifest should exit non-zero" >&2; exit 1
fi
rm -f /tmp/cf_badweight.txt

# 5. Fuzz the parser/pipeline under the sanitizers (short, deterministic run).
run "fuzz (150 iterations)" ./scripts/fuzz.sh 150 1

printf '\nAll checks passed.\n'
