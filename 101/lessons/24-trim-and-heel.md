# Trim and heel

A ship is never perfectly level. Cargo placed too far forward or aft causes one end to sink deeper than the other — that is **trim**. Cargo placed off the centreline causes one side to dip — that is **heel**. CargoForge-C computes both quantities inside `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c), and both appear in the printed loading plan and in every output format. Understanding how the code derives them turns stability numbers from opaque floats into quantities you can reason about and trust.

<svg viewBox="0 0 600 200" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:580px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Trim is fore-aft tilt; heel is side-to-side tilt</title>
<desc>Left, a side view: the ship sits deeper aft than forward — trim by the stern, draft aft greater than draft fore. Right, a front view: the ship leans to one side — heel, or list. The waterline stays horizontal in both; it is the ship that tilts.</desc>
<text x="40" y="30" fill="currentColor" font-size="12" font-weight="600">TRIM · side view</text>
<text x="370" y="30" fill="currentColor" font-size="12" font-weight="600">HEEL · front view</text>
<!-- TRIM panel -->
<rect x="40" y="116" width="244" height="62" fill="#12A594" fill-opacity="0.06"/>
<line x1="40" y1="116" x2="284" y2="116" stroke="#12A594" stroke-width="1.1" opacity="0.5"/>
<path d="M58,100 L262,86 L258,140 L62,156 Z" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-width="1.3" opacity="0.8"/>
<line x1="74" y1="116" x2="74" y2="153" stroke="#12A594" stroke-width="2"/><text x="74" y="170" fill="#12A594" font-size="10" text-anchor="middle">aft</text>
<line x1="246" y1="116" x2="246" y2="129" stroke="#12A594" stroke-width="2"/><text x="246" y="170" fill="#12A594" font-size="10" text-anchor="middle">fore</text>
<text x="162" y="60" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.65">draft aft &gt; draft fore</text>
<!-- HEEL panel -->
<rect x="320" y="116" width="244" height="62" fill="#12A594" fill-opacity="0.06"/>
<line x1="320" y1="116" x2="564" y2="116" stroke="#12A594" stroke-width="1.1" opacity="0.5"/>
<path d="M372,78 L398,148 Q430,170 466,158 Q506,146 516,110" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-width="1.3" opacity="0.8"/>
<line x1="372" y1="78" x2="516" y2="110" stroke="currentColor" stroke-width="1.3" opacity="0.8"/>
<text x="444" y="60" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.65">lists to one side</text>
<text x="300" y="194" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.6">The waterline stays horizontal — it is the ship that tilts. Both come from where the cargo sits.</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Trim** = "one end of the ship sits deeper in the water than the other" — caused by the longitudinal centre of gravity (LCG) being off-centre fore-or-aft; `perform_analysis` reports this as `r.trim` in metres by stern.
- **Heel** = "the ship leans to one side" — caused by the transverse centre of gravity (TCG) sitting to port or starboard of the centreline; `perform_analysis` reports this as `r.heel` in degrees.
- **LCG / TCG** = "where the loaded ship's combined weight is centred, measured along the ship's length (LCG) or across its width (TCG)" — both are computed by dividing the total weight-moment by total displacement in the single-pass loop inside `analysis.c`.
- **GM (corrected)** = "how stiff the ship is against tipping sideways, after accounting for sloshing liquid in tanks" — the heel formula uses `gm_effective` (the free-surface-corrected GM) because using the raw GM would underestimate how much the ship actually leans.
- **`place_cargo_3d` vs `perform_analysis`** = "the bin-packer fills the holds (ForwardHold, AftHold, Deck) without caring about balance; `perform_analysis` measures the result afterward" — if trim or heel comes out bad, the operator must manually re-assign items between bins and re-run the analysis.
- **CG percentage display** = "how far through the ship's length and beam the cargo mass sits, expressed as a percentage" — `perc_x` 45–55 % and `perc_y` 40–60 % is the acceptable band; values outside it trigger a "Warning" in the printed loading plan.

**Why it matters:** A ship with too much trim by stern steers poorly and may ship water over the stern; a ship with excessive heel risks cargo shifting or, at the extreme, capsizing. Both conditions are invisible until you run `perform_analysis` — which is why the analysis step is not optional after `place_cargo_3d`.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> Imagine balancing a plank on two pencils: one pencil runs fore-to-aft under the centre (the longitudinal axis), and one runs side-to-side (the transverse axis). Slide cargo toward the stern — the plank tips down aft: that is trim. Pile cargo on the starboard side — the plank tips right: that is heel. The plank only levels out when the weight sits directly above the crossing point of the two pencils.

In CargoForge-C the "crossing point" is the ship's metacentre — the height above the keel where the righting force acts. `perform_analysis` measures where the cargo mass actually landed (`r.lcg` fore-aft, `tcg` side-to-side) and compares it to where the hull wants it to be. The stiffer the ship (higher `gm_effective`), the less it heels for a given offset. The bins — ForwardHold, AftHold, Deck — are just where `place_cargo_3d` parked the weight; `perform_analysis` is the moment you check whether the plank still balances.

---

## The two axes of imbalance

Think of a ship floating on a pivot point. There are two independent ways the vessel can rotate:

- **Longitudinal rotation (trim):** rotation around the transverse axis — bow rises and stern sinks, or vice versa.
- **Transverse rotation (heel / list):** rotation around the longitudinal axis — port side dips or starboard side dips.

Both rotations are governed by the same underlying principle: if the vertical line through the centre of gravity (G) does not pass through the centre of buoyancy (B), the hull rotates until it does.

---

## Trim

### Why it happens

Trim arises from a mismatch between the **longitudinal centre of gravity (LCG)** and the **longitudinal centre of buoyancy (LCB)**. The LCB is the horizontal centroid of the underwater volume; for a symmetric, freely floating hull it coincides with the centroid of the waterplane — roughly midship. If the loaded LCG sits aft of the LCB, the stern sinks and the bow lifts; that is *trim by stern*. The reverse gives *trim by head*.

### The relevant distances

| Symbol | Meaning |
|--------|---------|
| $LCG$ | Longitudinal CG measured from midship (m); positive = aft |
| $BM_L$ | Longitudinal metacentric radius (m) |
| $GM_L$ | Longitudinal metacentric height = $KB + BM_L - KG$ |
| $L$ | Ship length (m) |

The restoring moment per unit trim angle is proportional to $GM_L \times \text{displacement}$. For a small trim $\delta$ (in metres by stern) this gives:

$$\text{trim} = \frac{LCG \times L}{GM_L}$$

A positive result means trim by stern (stern deeper); a negative result means trim by head.

### How `analysis.c` computes it

In the single-pass accumulation loop ([`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c), lines 102–117), each placed cargo item contributes to a **moment about midship**:

```c
/* from src/analysis.c */
lcg_moment += c->weight * (cx - ship->length / 2.0f);
```

Here `cx` is the longitudinal centre of the item. After the loop, the LCG in metres from midship is:

```c
r.lcg = lcg_moment / displacement_kg;
```

Next, the code derives $BM_L$ — the longitudinal equivalent of the transverse metacentric radius. When a hydrostatic table is loaded, $BM_L$ is back-calculated from the tabulated MTC (moment to change trim one centimetre):

```c
/* Table-based: back-calculate BM_L from MTC */
bm_l = he.mtc * 100.0f * ship->length / displacement_t;
```

Without a table (box-hull fallback), the second moment of area of the waterplane about the transverse axis gives:

```c
/* Box-hull fallback */
float inertia_l = (ship->width * powf(ship->length, 3) / 12.0f) * WATERPLANE_COEFF;
bm_l = inertia_l / displaced_vol;
```

The waterplane coefficient `WATERPLANE_COEFF = 0.85` corrects for the fact that real ships taper at bow and stern — the waterplane is not a perfect rectangle.

Finally, $GM_L$ and trim:

```c
float gm_l = r.kb + bm_l - r.kg;
if (gm_l > 0.01f)
    r.trim = r.lcg * ship->length / gm_l;
```

The guard `gm_l > 0.01f` prevents division by near-zero: a longitudinally unstable ship (extremely unlikely in practice) would be a separate safety problem, not a trim to report.

The result is stored in `AnalysisResult.trim` (metres by stern) and printed as:

```
Trim (by stern)       : 0.423 m
```

!!! note "Sign convention"
    Positive `trim` means the stern is deeper than the bow. A negative value means trim by head (bow deeper). Both can be acceptable operationally, but large trim by head degrades manoeuvring.

---

## Heel

### Why it happens

Heel arises from a **transverse CG offset** (TCG) — the loaded centre of gravity sitting to port or starboard of the centreline. The hull rotates until the transverse metacentric restoring moment exactly balances the heeling moment produced by that offset.

### The small-angle formula

For small angles (under about 10°), the restoring arm is approximately $GM_\text{corrected} \times \sin(\theta)$. At equilibrium:

$$\tan(\theta_\text{heel}) = \frac{TCG}{GM_\text{corrected}}$$

so the heel angle in degrees is:

$$\theta_\text{heel} = \arctan\!\left(\frac{TCG}{GM_\text{corrected}}\right) \times \frac{180°}{\pi}$$

TCG is the transverse distance of the combined CG from the ship's centreline (half-beam). A positive TCG is to starboard; a negative TCG is to port.

### How `analysis.c` computes it

The transverse moment accumulates alongside the longitudinal moment in the same cargo loop:

```c
/* from src/analysis.c */
moment_y += c->weight * cy;
```

where `cy = pos_y + dimensions[1] / 2.0f` is the transverse centre of the item in ship coordinates (0 = port side of hull). After the loop, the code converts this to an offset from the centreline:

```c
/* from src/analysis.c */
float avg_y = moment_y / r.total_cargo_weight_kg;
tcg = avg_y - ship->width / 2.0f;
```

`ship->width / 2.0f` is the half-beam — the centreline position in the ship's coordinate system. A positive `tcg` means CG is to starboard of centre.

Then:

```c
if (gm_effective > 0.01f)
    r.heel = atanf(tcg / gm_effective) * 180.0f / (float)M_PI;
```

`gm_effective` is `gm_corrected` — the GM after the free-surface correction — because sloshing liquid in tanks makes the ship less stiff, and that reduced stiffness affects how much the hull heels for a given CG offset. Using the corrected GM here is not optional; omitting it would underestimate heel.

The result lands in `AnalysisResult.heel` (degrees) and is printed as:

```
Heel                  : 2.14 deg
```

!!! warning "When GM is very small"
    Both formulas contain a guard `gm_effective > 0.01f`. If corrected GM is near zero, the ship is tender — the heel formula would produce an absurdly large angle and the small-angle approximation would be invalid anyway. CargoForge-C leaves `r.heel = 0.0f` in that case and separately flags the GM through the IMO criterion `IMO_GM_MIN = 0.15 m`.

---

## CG percentage display

In addition to the raw LCG and TCG values, `perform_analysis` computes the cargo CG as percentages of the ship's length and beam:

```c
r.cg.perc_x = (moment_x / r.total_cargo_weight_kg) / ship->length * 100.0f;
r.cg.perc_y = (moment_y / r.total_cargo_weight_kg) / ship->width  * 100.0f;
```

These are cargo-only percentages — they tell you where the cargo mass sits relative to the hull box, not where the combined (lightship + cargo + ballast) CG sits. The print routine in `analysis.c` uses simple thresholds to label the CG quality:

```c
const char *cg_str = (a.cg.perc_x >= 45 && a.cg.perc_x <= 55 &&
                       a.cg.perc_y >= 40 && a.cg.perc_y <= 60) ? "Good" : "Warning";
```

50 % longitudinal and 50 % transverse would be perfectly centred. The acceptable band is deliberately wider transversely (40–60 %) than longitudinally (45–55 %) because a modest heel is operationally more tolerable than a pronounced trim.

---

## How trim and heel feed the output

The `print_loading_plan` function in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) surfaces both values in the *Load Summary* and *Stability* sections:

```
CG (Lon / Trans)      : 51.3% / 49.1% | Good
...
Trim (by stern)       : 0.423 m
Heel                  : 2.14 deg
```

In JSON mode ([`src/json_output.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/json_output.c)), the same fields appear as `"trim"` and `"heel"` in the top-level result object, and as `"trim_m"` and `"heel_deg"` in the public `CfResult` struct exposed by `libcargoforge.h`.

---

## What the bin-packer does about it

`place_cargo_3d` in [`src/placement_3d.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/placement_3d.c) uses a First-Fit-Decreasing strategy with three hard-coded bins (ForwardHold, AftHold, Deck). It does **not** attempt to minimise trim or heel during placement — that is a stowage plan, not a stability optimiser. The consequence is that a carelessly loaded manifest can produce trim and heel values that are technically within the code's constraints but still operationally undesirable. Running `perform_analysis` after placement tells you the result; it is then the operator's responsibility to adjust the manifest or the bin assignments.

!!! tip "Improving trim in practice"
    If the analysis reports excessive trim by stern, move heavy items from AftHold into ForwardHold. For heel, ensure the combined transverse moment is symmetric — the easiest fix is pairing heavy items on port and starboard within the same hold.

---

## Recap

- **Trim** = longitudinal imbalance. `r.lcg` (moment about midship / displacement) drives it; `r.trim = r.lcg × L / GM_L` gives metres by stern.
- **Heel** = transverse imbalance. `tcg = avg_y − B/2` gives the CG offset; `r.heel = arctan(tcg / GM_corrected)` gives degrees.
- Both calculations live in `perform_analysis` ([`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)) and share the single-pass cargo accumulation loop that also computes KG and displacement.
- Heel uses the **corrected** GM (`gm_corrected`, after free-surface correction) — this is not optional.
- The bin-packer places cargo without minimising trim or heel; `perform_analysis` reports the outcome afterward.
- Guards (`gm_l > 0.01f`, `gm_effective > 0.01f`) prevent division by near-zero when the ship is tender.

*Next: [Free surface and the IMO criteria](25-free-surface-and-imo-criteria.md).*
