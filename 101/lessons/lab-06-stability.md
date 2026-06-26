# Lab 6 — Compute GM, then break it

In lessons 17–23 you learned the physics. Here you close the loop: compute KB, BM, KG, and GM by hand for a small ship, predict the verdict before running the tool, then deliberately overload the deck until the tool reports `IMO NON-COMPLIANT`. Understanding the exact moment a ship tips from stable to unstable — and watching CargoForge-C catch it — is the best way to make those equations stick.

---

## Setup

From the repository root, build the binary if you have not already:

```bash
make
```

You should see a `cargoforge` binary appear in the project root. Verify it works:

```bash
./cargoforge version
```

Create a working directory for this lab:

```bash
mkdir -p /tmp/lab06
```

---

## Part 1 — Write the ship config

This lab uses a narrow coastal freighter — narrow beam means a smaller waterplane moment of inertia, so BM is modest and GM is easy to push negative.

Create `/tmp/lab06/ship.cfg`:

```
# Lab 06 ship — narrow coastal freighter
length_m=60
width_m=12
max_weight_tonnes=20000
lightship_weight_tonnes=3000
lightship_kg_m=7.0
```

Key numbers to remember: $L = 60\ \text{m}$, $B = 12\ \text{m}$, lightship $= 3000\ \text{t}$ with its centre of gravity at $KG_{\text{ls}} = 7.0\ \text{m}$ above keel.

---

## Part 2 — Hand-calculate GM for the baseline cargo

Create a single-item cargo manifest `/tmp/lab06/cargo_baseline.txt`:

```
# ID          Weight(t)  LxWxH      Type
DeepHoldItem   500       10x8x4     standard
```

Before running anything, work through the arithmetic. The box-hull model
(`perform_analysis` in `src/analysis.c`) uses these constants:

| Constant | Value | Meaning |
|---|---|---|
| $\rho_{\text{sw}}$ | 1.025 t/m³ | Seawater density |
| $C_b$ | 0.75 | Block coefficient |
| $C_w$ | 0.85 | Waterplane area coefficient |
| $KB\_FACTOR$ | 0.53 | $KB \approx 0.53 \times T$ |

### Step-by-step

**Displacement and volume**

$$\Delta = \Delta_{\text{ls}} + W_{\text{cargo}} = 3000 + 500 = 3500\ \text{t}$$

$$\nabla = \frac{\Delta}{\rho_{\text{sw}}} = \frac{3500}{1.025} = 3414.6\ \text{m}^3$$

**Draft (box-hull fallback)**

$$T = \frac{\nabla}{L \times B \times C_b} = \frac{3414.6}{60 \times 12 \times 0.75} = 6.323\ \text{m}$$

**Vertical centre of buoyancy**

$$KB = 0.53 \times T = 0.53 \times 6.323 = 3.351\ \text{m}$$

**Transverse metacentric radius**

$$I_T = \frac{L \times B^3}{12} \times C_w = \frac{60 \times 12^3}{12} \times 0.85 = 7344\ \text{m}^4$$

$$BM = \frac{I_T}{\nabla} = \frac{7344}{3414.6} = 2.151\ \text{m}$$

**Vertical centre of gravity**

The placement engine (`place_cargo_3d` in `src/placement_3d.c`) puts `DeepHoldItem`
into the `ForwardHold` bin. That hold has its floor at $z = -8\ \text{m}$.
An item of height $h = 4\ \text{m}$ sitting on the floor has its centroid at:

$$c_z = z_{\text{floor}} + \frac{h}{2} = -8 + 2 = -6\ \text{m}$$

The analysis code (`src/analysis.c`, line 99 and 115) sums the vertical moment
over lightship plus every placed cargo item:

$$KG = \frac{\Delta_{\text{ls}} \times KG_{\text{ls}} + W_{\text{cargo}} \times c_z}{\Delta}
     = \frac{3000 \times 7.0 + 500 \times (-6.0)}{3500}
     = \frac{18000}{3500} = 5.143\ \text{m}$$

**GM**

$$GM = KB + BM - KG = 3.351 + 2.151 - 5.143 = \mathbf{0.359\ \text{m}}$$

The IMO minimum is $GM \geq 0.15\ \text{m}$ (`IMO_GM_MIN` in `src/analysis.c`). Your
predicted verdict: **compliant**.

---

## Part 3 — Verify against the tool

Run the optimizer on your baseline files:

```bash
./cargoforge optimize --ship /tmp/lab06/ship.cfg \
                      --cargo /tmp/lab06/cargo_baseline.txt
```

Scan the output for the stability block. You should see something like:

```
GM (corrected):          0.36 m
IMO Intact Stability:    COMPLIANT
```

The tool's GM should match your hand-calculated 0.359 m to within rounding.

!!! note "No free-surface correction yet"
    The baseline has no tank CSV, so `ship->tanks` is NULL and `free_surface_correction`
    is exactly 0.0. That is why the hand calc and the tool agree precisely. Lab 8 adds
    tanks; the correction term becomes non-trivial there.

You can also inspect cargo positions with the `info` subcommand:

```bash
./cargoforge info --ship /tmp/lab06/ship.cfg \
                  --cargo /tmp/lab06/cargo_baseline.txt
```

Confirm `DeepHoldItem` shows a negative $z$ position (inside the hold, below waterline).

---

## Part 4 — Raise the cargo; watch GM fall

Now create a heavier, higher cargo manifest `/tmp/lab06/cargo_heavy_deck.txt`:

```
# ID           Weight(t)  LxWxH      Type
DeckPile        1000      10x8x4     standard
```

Before running it, predict the outcome. `DeckPile` is heavy — the optimizer will
try holds first, but `ForwardHold` has `max_weight = 0.30 × 20000 = 6000\ \text{t}`,
so weight is not the constraint that forces it onto the `Deck` bin. What matters
is that this manifest is intentionally designed to be placed on deck: when you
inspect the output you will see it there. The `Deck` bin floor is at $z = 0\ \text{m}$.

Centroid of `DeckPile` on the deck floor:

$$c_z = 0 + \frac{4}{2} = 2.0\ \text{m}$$

Repeat the calculation with $W_{\text{cargo}} = 1000\ \text{t}$, $\Delta = 4000\ \text{t}$:

$$\nabla = \frac{4000}{1.025} = 3902.4\ \text{m}^3, \quad T = \frac{3902.4}{60 \times 12 \times 0.75} = 7.227\ \text{m}$$

$$KB = 0.53 \times 7.227 = 3.830\ \text{m}$$

$$BM = \frac{7344}{3902.4} = 1.882\ \text{m}$$

$$KG = \frac{3000 \times 7.0 + 1000 \times 2.0}{4000} = \frac{23000}{4000} = 5.750\ \text{m}$$

$$GM = 3.830 + 1.882 - 5.750 = \mathbf{-0.038\ \text{m}}$$

$GM$ is negative. The ship is **unstable** — it will capsize rather than self-right.
Even the GZ curve already fails: the wall-sided formula gives

$$GZ(30°) = \sin(30°) \times \left[GM + BM \cdot \frac{\tan^2(30°)}{2}\right]
           = 0.5 \times \left[-0.038 + 1.882 \times 0.167\right] = 0.138\ \text{m}$$

That is below the IMO threshold of 0.20 m.

Run the tool to confirm:

```bash
./cargoforge optimize --ship /tmp/lab06/ship.cfg \
                      --cargo /tmp/lab06/cargo_heavy_deck.txt
```

Expected output excerpt:

```
GM (corrected):          -0.04 m
GZ at 30 deg:             0.14 m
IMO Intact Stability:    NON-COMPLIANT
```

!!! warning "Why adding MORE cargo can hurt stability"
    You added 500 t more cargo and GM dropped from +0.359 m to −0.038 m. This feels
    counterintuitive — more displacement should help, right? The problem is WHERE the
    weight is. The deck cargo raised $KG$ faster than the extra displacement raised $KB$.
    The increase in $BM$ from the higher displacement partially compensated, but not
    enough. This is the core reason stability booklets specify maximum deck-load heights,
    not just weights.

---

## Part 5 — Use `validate` to see the rejection without placing cargo

The `validate` subcommand parses files and reports structural problems, but does
not run the placement engine. It will not report a stability verdict (placement
must happen first), but it confirms the manifest is well-formed:

```bash
./cargoforge validate --ship /tmp/lab06/ship.cfg \
                      --cargo /tmp/lab06/cargo_heavy_deck.txt
```

Expected: no parse errors, exit code 0. The non-compliance is a physics result
of placement, not a format error — only `optimize` catches it.

---

## Part 6 — Run the test suite and sanitizers

The stability calculations you just verified by hand are covered by `test_analysis`.
Run the full suite:

```bash
make test
```

All 8 test binaries should exit clean. Now add AddressSanitizer and Undefined
Behaviour Sanitizer:

```bash
make test-asan
```

ASan instruments every heap allocation and catches use-after-free bugs (one of
which — the heap-use-after-free in `parse_cargo_list` — was caught exactly this
way during development; see `docs/BUG_JOURNAL.md` for the entry). UBSan catches
signed integer overflow and misaligned accesses in the floating-point arithmetic
paths that compute KB, BM, and KG.

---

## Part 7 — (Optional) JSON output

The same optimize run accepts a `--format json` flag:

```bash
./cargoforge optimize --ship /tmp/lab06/ship.cfg \
                      --cargo /tmp/lab06/cargo_heavy_deck.txt \
                      --format json | python3 -m json.tool | grep -E '"gm|"kb|"bm|"kg|"imo'
```

You will see the individual `kb`, `bm`, `kg`, `gm`, `gm_corrected`, and
`imo_compliant` fields from `AnalysisResult` serialized by `json_output.c`.
`imo_compliant` will be `0`.

---

## Solution

Full command sequence for both scenarios:

```bash
# Build
make

# Create files
mkdir -p /tmp/lab06

cat > /tmp/lab06/ship.cfg <<'EOF'
length_m=60
width_m=12
max_weight_tonnes=20000
lightship_weight_tonnes=3000
lightship_kg_m=7.0
EOF

cat > /tmp/lab06/cargo_baseline.txt <<'EOF'
DeepHoldItem   500   10x8x4   standard
EOF

cat > /tmp/lab06/cargo_heavy_deck.txt <<'EOF'
DeckPile   1000   10x8x4   standard
EOF

# Baseline — COMPLIANT, GM ≈ 0.36 m
./cargoforge optimize --ship /tmp/lab06/ship.cfg --cargo /tmp/lab06/cargo_baseline.txt

# Heavy deck — NON-COMPLIANT, GM ≈ -0.04 m
./cargoforge optimize --ship /tmp/lab06/ship.cfg --cargo /tmp/lab06/cargo_heavy_deck.txt

# Tests
make test
make test-asan
```

Expected key values:

| Scenario | $T$ (m) | $KB$ (m) | $BM$ (m) | $KG$ (m) | $GM$ (m) | IMO |
|---|---|---|---|---|---|---|
| 500 t in hold | 6.32 | 3.35 | 2.15 | 5.14 | **+0.36** | COMPLIANT |
| 1000 t on deck | 7.23 | 3.83 | 1.88 | 5.75 | **−0.04** | NON-COMPLIANT |

---

## Recap

- $GM = KB + BM - KG$. A ship becomes unstable the moment $GM$ drops below zero; IMO mandates $GM \geq 0.15\ \text{m}$ as a minimum safety margin.
- $BM = I_T / \nabla$. A narrow beam means a small $I_T$ and therefore a small $BM$ — less reserve against a rising $KG$.
- Adding cargo does not automatically improve stability. Deck cargo raises $KG$ faster than it raises $KB$, and the increase in $BM$ from extra displacement rarely compensates.
- CargoForge-C's `perform_analysis` in `src/analysis.c` evaluates all six IMO intact-stability criteria; `imo_compliant` is 0 if any single criterion fails.
- `make test-asan` is the way to run the same crash-detection the development team uses; it found the heap-use-after-free in `parse_cargo_list` that is documented in `docs/BUG_JOURNAL.md`.

*Next: [Config and manifest formats](26-config-and-manifest-formats.md).*
