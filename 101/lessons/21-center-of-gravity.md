# Center of gravity (KG)

Every stability calculation in CargoForge-C starts from one number: KG, the
height of the ship's combined centre of gravity above the keel. This lesson
explains what a centre of gravity is, how it is computed from first principles
using moments, why the vertical component (KG) is the one that dominates safety
decisions, and how `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) translates those
principles into the code that runs on every voyage plan.

## The mental model 🧠

Balance a loaded tray on one fingertip. The spot where it sits dead level — tipping toward neither the heavy teapot nor the empty cup — is the centre of gravity.

A ship's CG is that same balance point for thousands of tonnes. CargoForge-C finds it exactly the way you'd find the tray's: take each item's weight times its position, add them all up, and divide by the total weight — a **weighted average** (each weight × distance term is called a *moment*). `perform_analysis` does this for the lightship plus every placed cargo item, in all three axes at once.

**KG** is just the *vertical* coordinate of that balance point: how high it sits above the keel. And height is what bites. Slide one heavy crate from the hold up onto the deck and the teapot moves toward the rim of the tray — KG climbs, and the whole thing gets twitchy. Because stability is a tug-of-war between KG and the metacentre (Lesson 23), every metre you let KG rise is a metre of safety you spend.

<svg viewBox="0 0 600 216" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The centre of gravity is the weight-weighted average position</title>
<desc>Three cargo blocks of different weights sit at different positions along a beam. The balance point — the centre of gravity — is each weight times its distance, summed, divided by the total weight; it sits nearer the heaviest block.</desc>
<rect x="70" y="118" width="460" height="8" rx="2" fill="currentColor" opacity="0.5"/>
<rect x="118" y="92" width="44" height="26" rx="2" fill="#12A594" fill-opacity="0.14" stroke="#12A594" stroke-width="1.1"/><text x="140" y="84" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">1 t</text>
<rect x="266" y="56" width="68" height="62" rx="2" fill="#12A594" fill-opacity="0.14" stroke="#12A594" stroke-width="1.1"/><text x="300" y="48" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">3 t</text>
<rect x="412" y="74" width="56" height="44" rx="2" fill="#12A594" fill-opacity="0.14" stroke="#12A594" stroke-width="1.1"/><text x="440" y="66" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">2 t</text>
<path d="M320,126 L308,150 L332,150 Z" fill="#D05663"/>
<line x1="320" y1="118" x2="320" y2="92" stroke="#D05663" stroke-width="1" stroke-dasharray="3 2" opacity="0.7"/>
<text x="320" y="168" fill="#D05663" font-size="11" text-anchor="middle" font-weight="600">CG</text>
<text x="320" y="184" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.6">balance point — nearer the heaviest</text>
<text x="300" y="208" fill="currentColor" font-size="12.5" text-anchor="middle" opacity="0.75">CG = Σ(weightᵢ × positionᵢ) ⁄ Σ weightᵢ</text>
</svg>

<div class="lesson-widget" data-widget="cg-balance-beam"></div>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Centre of gravity** = "the single point where the whole ship's weight effectively acts" — not a physical object you can touch, but a calculated balance point that moves every time you load or shift a cargo item.
- **Moment** = "weight multiplied by height (or distance)" — it captures how much a heavy object at a given position "pulls" the balance point; a 20 t container high on deck contributes far more to the vertical moment than the same container sitting on the keel, because its arm (`cz`) is larger in `perform_analysis`'s accumulation loop.
- **KG** = "how high above the keel the ship's combined centre of gravity sits, in metres" — it is the single number that most directly controls whether the ship is safe, because it appears with a minus sign in `GM = KB + BM − KG`; every extra metre of KG shrinks GM by exactly one metre.
- **GM (metacentric height)** = "the gap between where gravity pulls and where the hull's buoyancy pushes back" — `perform_analysis` computes it as `r.gm = r.kb + r.bm - r.kg`; when that gap reaches zero the ship has no self-righting ability and will capsize.
- **`cz` = `pos_z + dimensions[2] / 2`** = "the arm used for each cargo box's vertical moment" — because a box's centre of gravity is at its geometric middle, not its bottom face; placing a 2 m box at z = 0 gives an arm of 1 m, not 0.
- **Free-surface correction** = "a penalty subtracted from GM to account for liquid sloshing in tanks" — `perform_analysis` applies it as `r.gm_corrected = r.gm - r.free_surface_correction`, and every downstream IMO check uses the corrected value, never the raw one.

**Why it matters:** if KG is computed incorrectly — wrong arm, skipped lightship seed, or an unplaced item sneaking into the loop — the GM the code reports will be wrong, and a ship that looks stable on screen may capsize at sea; the `pos_x < 0` guard in `perform_analysis` exists precisely to prevent unplaced cargo from corrupting this number.

---

## What is a centre of gravity?

The **centre of gravity** (CG) of a system of objects is the single point where
all the system's weight can be considered to act. It is a weighted average:
objects that are heavier pull the CG toward themselves; objects that are
lighter have less influence.

For a ship, the system has two major contributors:

1. **The lightship** — the hull, machinery, and fittings, whose weight and CG
   are measured once during a ship's inclining experiment and recorded.
2. **The cargo** — every item placed aboard, each with its own weight and
   position.

Tanks of ballast water and fuel are a third contributor; we will touch on them
at the end of this lesson.

---

## Moments: the arithmetic of "pulling"

To find the CG of a collection of objects you cannot simply average their
positions — a 1 000 t container at one end of the ship dominates a 10 t crate
at the other end. The correct operation is the **moment**:

$$\text{moment} = \text{weight} \times \text{arm}$$

where the **arm** is the distance from the object's position to a chosen
reference point (the origin).

The CG is then:

$$\text{CG} = \frac{\sum_i (w_i \times d_i)}{\sum_i w_i}$$

This formula says: sum all the individual moments, then divide by the total
weight. The result is the arm of the equivalent single weight.

!!! note "Why moments work"
    Imagine a see-saw balanced at its centre. If you place a 2 kg weight 1 m
    to the left, its moment is 2 N·m counterclockwise. A 1 kg weight 2 m to
    the right has moment 2 N·m clockwise — they balance. The pivot point is
    exactly where the formula above would put the combined CG.

---

## Three axes, one focus

A ship's CG has three components:

| Symbol | Axis | Reference |
|--------|------|-----------|
| LCG | Longitudinal (fore–aft) | Midship or aft perpendicular |
| TCG | Transverse (port–starboard) | Centreline |
| **KG** | **Vertical** | **Keel (K)** |

For intact-stability purposes, **KG dominates**. Here is why.

The ship's righting ability depends on the gap between G (centre of gravity)
and M (the transverse metacentre, a geometric property of the hull). That gap
is:

$$GM = KB + BM - KG$$

If KG rises — because heavy cargo is loaded high — GM shrinks. When GM reaches
zero the ship has no initial restoring force. If GM goes negative the ship
capsizes. Nothing in the formula involves LCG or TCG in the same direct way;
transverse offset causes heel, which can be corrected, but a high KG erodes
the fundamental righting lever.

---

## How CargoForge-C computes KG

Open [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c). The computation unfolds inside `perform_analysis` in a
single pass over the cargo array.

### Step 1 — seed with the lightship moment

Before looping over any cargo, the code seeds the vertical moment accumulator
with the lightship's known values:

```c
/* from src/analysis.c, perform_analysis() */
float vertical_moment = ship->lightship_weight * ship->lightship_kg;
```

`ship->lightship_weight` is in kilograms (converted from tonnes by the parser).
`ship->lightship_kg` is the height of the lightship CG above the keel, in
metres. Their product is the lightship's contribution to the total vertical
moment, in kg·m.

### Step 2 — accumulate cargo moments

The loop visits every cargo item. Unplaced items (those with `pos_x < 0`) are
skipped — they have not been assigned a physical location and therefore cannot
contribute to the moment.

```c
/* from src/analysis.c, perform_analysis() */
for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *c = &ship->cargo[i];
    if (c->pos_x < 0) continue;

    r.placed_item_count++;
    r.total_cargo_weight_kg += c->weight;

    float cx = c->pos_x + c->dimensions[0] / 2.0f;
    float cy = c->pos_y + c->dimensions[1] / 2.0f;
    float cz = c->pos_z + c->dimensions[2] / 2.0f;

    moment_x += c->weight * cx;
    moment_y += c->weight * cy;
    vertical_moment += c->weight * cz;
    lcg_moment += c->weight * (cx - ship->length / 2.0f);
}
```

Notice `cz`: the arm for the vertical moment is not `pos_z` alone, but
`pos_z + dimensions[2] / 2.0f`. A cargo item is a box; its centre of gravity
is at its geometric centre, which sits half a height above its bottom face.
The bottom face is at `pos_z`, so the centre is at `pos_z + height/2`.

For a 2 m × 2 m × 2 m box placed with its bottom at z = 0, the centre is at
z = 1 m — and its vertical moment contribution is `weight × 1.0`, not
`weight × 0`.

!!! example "A worked moment"
    Suppose a 20 t container (20 000 kg) with height 2.6 m is placed so that
    its base sits at z = 0 m (directly on the keel deck).

    - Arm = 0 + 2.6 / 2 = **1.3 m**
    - Moment = 20 000 × 1.3 = **26 000 kg·m**

    Another identical container stacked on top of it has its base at z = 2.6 m:

    - Arm = 2.6 + 1.3 = **3.9 m**
    - Moment = 20 000 × 3.9 = **78 000 kg·m**

    Stacking doubles the height of the upper box's CG but triples its moment.
    This is why high stowage rapidly erodes GM.

### Step 3 — add tank moments

If tanks are configured, their weight and vertical moment are folded in after
the cargo loop:

```c
/* from src/analysis.c, perform_analysis() */
if (ship->tanks && ship->tanks->count > 0) {
    tank_weight_t = calculate_tank_weight(ship->tanks);
    displacement_kg += tank_weight_t * 1000.0f;
    vertical_moment += calculate_tank_vertical_moment(ship->tanks) * 1000.0f;
}
```

`calculate_tank_vertical_moment` (in `tanks.c`) returns a moment in tonne·m;
multiplying by 1 000 converts it to kg·m for consistency.

### Step 4 — divide to get KG

After the accumulation is complete and the overweight check passes, one line
computes KG:

```c
/* from src/analysis.c, perform_analysis() */
r.kg = vertical_moment / displacement_kg;
```

This is the formula $KG = \Sigma(w_i \times z_i) / \Sigma w_i$ evaluated
over lightship + cargo + tanks in a single division.

---

## From KG to GM

With KG in hand, `perform_analysis` proceeds to hydrostatics to obtain KB and
BM (covered in Lesson 22), then applies the fundamental stability equation:

```c
/* from src/analysis.c, perform_analysis() */
r.gm = r.kb + r.bm - r.kg;
```

The free-surface correction (Lesson 23) is subtracted next:

```c
r.gm_corrected = r.gm - r.free_surface_correction;
```

All downstream criteria — the six IMO thresholds, trim, heel, GZ curve — use
`gm_corrected`, never the uncorrected `r.gm`. KG is the single value that
makes or breaks the chain.

---

## Why a high KG is dangerous

Consider what happens as KG rises while KB and BM remain fixed (same draft,
same hull):

$$GM = KB + BM - KG$$

Every extra metre of KG subtracts directly from GM. Below GM = 0 the ship is
unstable at rest: any small disturbance produces a heeling moment that the
ship cannot oppose, and it capsizes without external help.

Even before GM reaches zero, a low positive GM means:
- GZ at 30° falls below the IMO minimum of 0.20 m.
- The area under the GZ curve (energy stored as righting work) shrinks.
- A breaking wave, a shifted cargo load, or a flooding compartment can push
  the ship past its range of positive stability.

The console report produced by `print_loading_plan` shows this explicitly:

```
Stability
  KG / KB / BM          : 6.84 / 3.71 / 4.52 m
  GM (transverse)       : 1.39 m | Optimal
```

The label `"CRITICAL - Too tender"` appears when `gm_corrected < 0.3f` — a
value chosen to warn operators well before the IMO hard floor of 0.15 m.

---

## Recap

- The centre of gravity is a weighted average of positions, computed via
  **moments** (weight × arm).
- CargoForge-C seeds the vertical moment with the lightship contribution, then
  accumulates `weight × (pos_z + height/2)` for every placed cargo item.
- **KG** is the vertical CG measured from the keel; it is the single most
  influential number in the stability chain because it appears directly in
  $GM = KB + BM - KG$.
- A high KG reduces GM; when GM reaches zero the ship has no self-righting
  ability and capsizes.
- `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) computes KG in one line —
  `r.kg = vertical_moment / displacement_kg` — after a single loop that
  accumulates moments from lightship, cargo, and tanks.
- The corrected GM (`gm_corrected = gm - free_surface_correction`) is used for
  all IMO checks and downstream calculations; KG is its primary driver.

*Next: [The center of buoyancy and the metacenter](22-buoyancy-and-the-metacenter.md).*
