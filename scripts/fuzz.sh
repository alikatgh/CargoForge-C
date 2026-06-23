#!/usr/bin/env bash
# scripts/fuzz.sh - Random-input fuzzer for the CargoForge-C parser/pipeline.
#
# Throws many malformed and adversarial ship-config + cargo-manifest inputs at the
# AddressSanitizer/UBSan build and fails if any run crashes (killed by a signal) or
# trips a sanitizer. A clean error exit (parse rejected) is a PASS — the contract is
# "never crash on bad input," not "accept everything".
#
# Built to catch the bug class this project has actually hit: memory errors on the
# parser's failure paths (the double-free was found exactly this way, by hand).
#
# Usage: scripts/fuzz.sh [iterations] [seed]   (defaults: 300 iterations, seed 1)
# Deterministic by default (fixed seed) so CI runs are reproducible.
set -u
cd "$(dirname "$0")/.."

ITERS="${1:-300}"
RANDOM="${2:-1}"

SAN=./cargoforge-san
[ -x "$SAN" ] || make sanitize >/dev/null
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1"

cfg=$(mktemp); manifest=$(mktemp); err=$(mktemp)
trap 'rm -f "$cfg" "$manifest" "$err"' EXIT

KEYS=(length_m width_m max_weight_tonnes lightship_weight_tonnes lightship_kg_m holds bogus_key)
VALS=(150 25 -5 0 0.0 abc 1e9 99999999999 3.14 "" "  " 2 50 1000000000000)
TYPES=(general bulk reefer "" hazardous)

# Echo one randomly-chosen argument (portable: indexes the positional params, so
# it works on bash 3.2 without namerefs). Call as: rand_of "${ARRAY[@]}".
rand_of() { echo "${@:$((RANDOM % $# + 1)):1}"; }

gen_config() {
    : > "$cfg"
    local lines=$((RANDOM % 8))
    for _ in $(seq 0 "$lines"); do
        local k v; k=$(rand_of "${KEYS[@]}"); v=$(rand_of "${VALS[@]}")
        case $((RANDOM % 4)) in
            0) printf '%s=%s\n' "$k" "$v" >> "$cfg" ;;
            1) printf '%s\n' "$k" >> "$cfg" ;;            # no '='
            2) printf '#%s=%s\n' "$k" "$v" >> "$cfg" ;;   # comment
            3) printf '%s=%s=%s\n' "$k" "$v" "$v" >> "$cfg" ;;
        esac
    done
}

gen_manifest() {
    : > "$manifest"
    local lines=$((RANDOM % 8))
    for _ in $(seq 0 "$lines"); do
        local w t; w=$(rand_of "${VALS[@]}"); t=$(rand_of "${TYPES[@]}")
        case $((RANDOM % 5)) in
            0) printf 'ID%d %s %sx%sx%s %s\n' "$RANDOM" "$w" "$w" "$w" "$w" "$t" >> "$manifest" ;;
            1) printf 'ID%d %s\n' "$RANDOM" "$w" >> "$manifest" ;;        # too few fields
            2) printf '# comment %d\n' "$RANDOM" >> "$manifest" ;;
            3) head -c $((RANDOM % 400 + 1)) /dev/urandom | tr -d '\0\n' >> "$manifest"; printf '\n' >> "$manifest" ;;
            4) printf 'ID%d %s %s %s\n' "$RANDOM" "$w" "$w" "$t" >> "$manifest" ;;  # bad dims
        esac
    done
}

fail=0
for i in $(seq 1 "$ITERS"); do
    gen_config; gen_manifest
    flags=""; [ $((RANDOM % 2)) -eq 0 ] && flags="--json"
    [ $((RANDOM % 3)) -eq 0 ] && flags="$flags --strict"
    "$SAN" $flags "$cfg" "$manifest" >/dev/null 2>"$err"
    rc=$?
    if [ "$rc" -ge 128 ] || grep -qiE 'AddressSanitizer|runtime error:|UndefinedBehavior' "$err"; then
        echo "FUZZ FAIL (iter $i, exit $rc):" >&2
        echo "--- config ---"   >&2; cat "$cfg" >&2
        echo "--- manifest ---" >&2; cat "$manifest" >&2
        echo "--- stderr ---"   >&2; cat "$err" >&2
        fail=1; break
    fi
done

if [ "$fail" -eq 0 ]; then
    echo "Fuzz OK: $ITERS iterations, no crashes or sanitizer errors."
else
    exit 1
fi
