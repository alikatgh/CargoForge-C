#!/usr/bin/env bash
# scripts/fuzz.sh - Random-input fuzzer for the CargoForge parser/pipeline.
#
# Builds an AddressSanitizer + UBSan binary, then throws many malformed and
# adversarial ship configs and cargo manifests (including dangerous-goods
# attributes and CSV-table config keys) at the `optimize` and `validate`
# subcommands. Fails if any run crashes (killed by a signal) or trips a sanitizer.
# A clean error exit (input rejected) is a PASS — the contract is "never crash on
# bad input", not "accept everything".
#
# Usage: scripts/fuzz.sh [iterations] [seed]   (defaults: 300 iterations, seed 1)
# Deterministic by default (fixed seed) so CI runs are reproducible.
set -u
cd "$(dirname "$0")/.."

ITERS="${1:-300}"
RANDOM="${2:-1}"

SAN=build/cargoforge-asan
mkdir -p build
if [ ! -x "$SAN" ]; then
    echo "Building sanitized binary ($SAN)..."
    cc -O1 -g -std=c99 -D_POSIX_C_SOURCE=200809L -Iinclude \
       -fsanitize=address,undefined -fno-omit-frame-pointer src/*.c -lm -o "$SAN" || exit 1
fi
export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"

cfg=$(mktemp); manifest=$(mktemp); err=$(mktemp)
trap 'rm -f "$cfg" "$manifest" "$err"' EXIT

CKEYS=(length_m width_m max_weight_tonnes lightship_weight_tonnes lightship_kg_m \
       permissible_sf_tonnes hydrostatic_table tank_config bogus_key)
VALS=(150 25 -5 0 0.0 abc 1e9 99999999999 3.14 "" "  " /nonexistent.csv)
TYPES=(standard bulk reefer hazardous "" oog)
DG=("DG:3.1:UN1203:A:F-E,S-D" "DG:5.1:UN1942:A:F-A,S-Q" "DG:::" "DG:9:UN9999:X:" "DG:abc" "")

# Echo one randomly-chosen argument (portable: indexes the positional params, so it
# works on bash 3.2 without namerefs). Call as: rand_of "${ARRAY[@]}".
rand_of() { echo "${@:$((RANDOM % $# + 1)):1}"; }

gen_config() {
    : > "$cfg"
    local n=$((RANDOM % 8)) k v
    for _ in $(seq 0 "$n"); do
        k=$(rand_of "${CKEYS[@]}"); v=$(rand_of "${VALS[@]}")
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
    local n=$((RANDOM % 8)) w t d
    for _ in $(seq 0 "$n"); do
        w=$(rand_of "${VALS[@]}"); t=$(rand_of "${TYPES[@]}"); d=$(rand_of "${DG[@]}")
        case $((RANDOM % 5)) in
            0) printf 'ID%d %s %sx%sx%s %s %s\n' "$RANDOM" "$w" "$w" "$w" "$w" "$t" "$d" >> "$manifest" ;;
            1) printf 'ID%d %s\n' "$RANDOM" "$w" >> "$manifest" ;;      # too few fields
            2) printf '# comment %d\n' "$RANDOM" >> "$manifest" ;;
            3) head -c $((RANDOM % 400 + 1)) /dev/urandom | tr -d '\0\n' >> "$manifest"; printf '\n' >> "$manifest" ;;
            4) printf 'ID%d %s %s %s\n' "$RANDOM" "$w" "$w" "$t" >> "$manifest" ;;  # bad dims
        esac
    done
}

fail=0
for i in $(seq 1 "$ITERS"); do
    gen_config; gen_manifest
    sub=optimize; [ $((RANDOM % 3)) -eq 0 ] && sub=validate
    flags=""; [ $((RANDOM % 2)) -eq 0 ] && flags="--format=json"
    "$SAN" "$sub" $flags "$cfg" "$manifest" >/dev/null 2>"$err"
    rc=$?
    if [ "$rc" -ge 128 ] || grep -qiE 'AddressSanitizer|runtime error:|UndefinedBehavior' "$err"; then
        echo "FUZZ FAIL (iter $i, sub=$sub, exit $rc):" >&2
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
