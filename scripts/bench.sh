#!/usr/bin/env sh
# scripts/bench.sh - Throughput benchmark for the placement + analysis pipeline.
#
# Generates a synthetic manifest of N cargo items and times a full run. Use it to
# spot performance regressions (the placement CG evaluation is the hot path).
#
# Usage: scripts/bench.sh [N]   (default N = 2000 items)
set -eu
cd "$(dirname "$0")/.."

N="${1:-2000}"
[ -x ./cargoforge ] || make --silent

ship=$(mktemp); manifest=$(mktemp)
trap 'rm -f "$ship" "$manifest"' EXIT

printf 'length_m=300\nwidth_m=40\nmax_weight_tonnes=2000000\nlightship_weight_tonnes=20000\nlightship_kg_m=10\nholds=8\n' > "$ship"
awk 'BEGIN { for (i = 0; i < '"$N"'; i++)
        printf "Item%d %d %dx%dx%d general\n", i, (i%50)+1, (i%8)+2, (i%4)+2, (i%3)+2 }' > "$manifest"

printf 'Benchmark: %d cargo items\n' "$N"
# The shell `time` builtin reports real/user/sys; stderr carries placement warnings.
time ./cargoforge "$ship" "$manifest" >/dev/null 2>/dev/null
printf 'done.\n'
