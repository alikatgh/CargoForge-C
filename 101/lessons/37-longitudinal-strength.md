# Longitudinal strength

A ship is not rigid. Under a heavy concentrated cargo load the hull can hog — arch upward amidships like a banana — or sag when the ends carry more weight than the middle. Both modes stress the hull steel, and class-society rules define how far each can go before the structure is in danger. CargoForge-C computes the **still-water shear force (SWSF)** and **still-water bending moment (SWBM)** at every hull station and compares the results against the permissible limits stored in the ship config. This lesson traces that calculation from the first array to the final compliance flag.

## The mental model 🧠

A loaded hull is a very long beam, and like any beam it can be bent by an uneven load. Weight pushes *down* wherever cargo sits; buoyancy pushes *up* spread along the submerged length; wherever those two do not match station-for-station, the leftover force tries to bend the steel. Pile weight amidships and the ends float up — the hull *sags*; load the ends heavy and the middle rides up — it *hogs*. Both are the hull flexing like a banana.

CargoForge measures it the way structural engineers do: take the net load (weight minus buoyancy) along the length, add it up running fore-to-aft to get the **shear force**, then add *that* up to get the **bending moment** — the integral of an integral. The peak bending moment is compared against the class-society limit in the ship config, and a stow that crams everything into one hold can pass every stability check and still *fail here* — because a ship that floats perfectly upright can still break its back. Spread the load and you protect the steel.

<svg viewBox="0 0 580 250" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Longitudinal strength: net load along the hull integrates to a bending moment, checked against limits</title>
<desc>Top: along the hull, the stepped weight distribution and the smooth buoyancy distribution differ — their difference is the net load. Bottom: integrating the net load twice gives the still-water bending moment, a hump along the length, which is compared against the permissible class limit.</desc>
<text x="40" y="22" fill="currentColor" font-size="11" font-weight="600">weight vs buoyancy along the hull</text>
<line x1="60" y1="80" x2="520" y2="80" stroke="currentColor" stroke-width="1" opacity="0.35"/>
<path d="M70,72 Q290,42 510,72" fill="none" stroke="#12A594" stroke-width="1.6"/>
<text x="118" y="54" fill="#12A594" font-size="9.5">buoyancy</text>
<path d="M70,74 L150,74 L150,60 L250,60 L250,86 L350,86 L350,56 L450,56 L450,76 L510,76" fill="none" stroke="currentColor" stroke-width="1.4" opacity="0.78"/>
<text x="360" y="48" fill="currentColor" font-size="9.5" opacity="0.7">weight</text>
<text x="40" y="138" fill="currentColor" font-size="11" font-weight="600">→ still-water bending moment</text>
<line x1="60" y1="204" x2="520" y2="204" stroke="currentColor" stroke-width="1" opacity="0.35"/>
<line x1="60" y1="168" x2="520" y2="168" stroke="#D05663" stroke-width="1" stroke-dasharray="4 3" opacity="0.7"/>
<text x="520" y="164" fill="#D05663" font-size="9.5" text-anchor="end">permissible limit</text>
<path d="M70,204 Q290,150 510,204" fill="none" stroke="#12A594" stroke-width="1.9"/>
<text x="290" y="236" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.65">21 stations; integrate net load → shear → bending; compare to class limits (<tspan font-family="var(--md-code-font,monospace)">longitudinal_strength.c</tspan>).</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Still-water bending moment (SWBM)** = "how much the hull is trying to curve when the ship is sitting still in calm water" — it is the quantity CargoForge-C's `calculate_longitudinal_strength` ultimately computes and compares against class-society limits, because a hull that curves too much will crack.
- **Hogging vs sagging** = "banana-up vs banana-down" — hogging happens when cargo is piled amidships and the ends are light; sagging happens when the ends are heavy. The code tracks them separately (`max_bm_hog` and `max_bm_sag`) because hull steel resists the two modes differently.
- **Net load at a station** = "weight minus buoyancy at that slice of the ship" — `r.net_load[i] = r.weight_dist[i] - r.buoyancy_dist[i]` is where all the careful distribution work upstream collapses into a single signed number; positive means the water isn't pushing hard enough at that point.
- **Integrate twice** = "add up the running imbalance, then add that up again" — the two trapezoidal loops turn net-load tonnes into shear-force tonnes and then into bending-moment tonne-metres; each step multiplies by a length, which is why the units change and why both arrays start and end at zero.
- **Trapezoidal shape, then normalise** = "draw a rough curve first, then stretch it to hit the right total" — `distribute_lightship` and `distribute_buoyancy` both build a plausible shape and then rescale so the integral equals the actual lightship weight or displacement; this two-step pattern appears in every distribution function in the file.
- **`check_strength_limits` return values (1 / 0 / −1)** = "pass / fail / not checked" — when `strength_limits` is `NULL` in the `Ship` struct because the config omitted the three permissible-limit keys, the check is simply skipped and `strength_compliant` stays −1; a missing config silently produces an optimistic result.

**Why it matters:** if the bending moment exceeds the permissible limit the hull steel can buckle or fracture; and because unplaced cargo items are silently excluded from `distribute_cargo`, a plan with many unplaced items will under-report the SWBM — making a dangerously loaded ship look compliant.

---

## The physical picture

Imagine the ship as a beam floating on water. At every cross-section along its length there is some weight acting downward (hull steel, machinery, cargo) and some buoyancy acting upward (the upward pressure of displaced water). Where weight exceeds buoyancy the section is loaded in excess; where buoyancy exceeds weight it is relieved. The difference — the **net load** — varies continuously along the ship.

Two cumulative integrals turn net load into the quantities that structurally matter:

$$\text{Shear force}(x) = \int_0^x \bigl( w(\xi) - b(\xi) \bigr)\, d\xi$$

$$\text{Bending moment}(x) = \int_0^x \text{Shear force}(\xi)\, d\xi$$

The shear force is the tendency of one cross-section to slide past the next. The bending moment is the tendency of the hull to curve. Both are zero at the free ends (AP and FP) of a freely floating beam and reach their peaks somewhere amidships.

## 20 stations, 21 values

`longitudinal_strength.c` discretises the hull into `LS_NUM_STATIONS = 20` evenly-spaced stations (the constant comes from `longitudinal_strength.h`). Because the stations run from AP (index 0) to FP (index 19), there are 20 points producing 20 intervals.

Station positions are set up in `calculate_longitudinal_strength`:

```c
/* from src/longitudinal_strength.c */
float dx = ship_length / (float)(LS_NUM_STATIONS - 1);
for (int i = 0; i < LS_NUM_STATIONS; i++) {
    r.station_pos[i] = i * dx;
}
```

With a 200 m ship `dx` = 200 / 19 ≈ 10.53 m. Each element of the `weight_dist`, `buoyancy_dist`, `net_load`, `shear_force`, and `bending_moment` arrays corresponds to one of these station positions.

## Distributing weight

Two static helpers fill `weight_dist[]`. They are called in sequence so their contributions simply add.

### Lightship weight — trapezoidal profile

A ship is not a uniform beam. There is more steel amidships (engine room, double-bottom structure) and lighter framing near the ends. `distribute_lightship` approximates this with a three-zone trapezoid:

| Zone | Fraction of length | Factor applied to average weight/metre |
|---|---|---|
| Stern (AP to 20% L) | 0–0.2 | Ramp from 0.6× to 1.0× |
| Amidships (20% to 80% L) | 0.2–0.8 | Constant 1.15× |
| Bow (80% L to FP) | 0.8–1.0 | Ramp from 1.0× to 0.5× |

```c
/* from src/longitudinal_strength.c */
float avg_wt = lightship_t / length; /* average weight per metre */

if (frac < 0.2f) {
    float t = frac / 0.2f;
    raw[i] = avg_wt * (0.6f + 0.4f * t);
} else if (frac < 0.8f) {
    raw[i] = avg_wt * 1.15f;
} else {
    float t = (frac - 0.8f) / 0.2f;
    raw[i] = avg_wt * (1.0f - 0.5f * t);
}
```

After the first pass the raw array is normalised by trapezoidal integration so its total equals the actual lightship weight in tonnes. This two-step pattern — compute a shape, then scale to the target total — appears in all three distribution functions.

### Cargo weight — uniform over each item's footprint

`distribute_cargo` converts each placed cargo item (those with `pos_x >= 0`) into a weight-per-metre value and spreads it across whatever stations the item overlaps:

```c
/* from src/longitudinal_strength.c */
float wt_per_m = cargo_t / cargo_length;

float overlap_lo = fmaxf(s_lo, x_start);
float overlap_hi = fminf(s_hi, x_end);

if (overlap_hi > overlap_lo) {
    float overlap_len = overlap_hi - overlap_lo;
    dist[s] += wt_per_m * overlap_len / dx;
}
```

Each station "owns" a band of ±dx/2 around it (with boundary corrections at the ends). The overlap between a cargo item's longitudinal extent and a station's band, divided by `dx`, gives the fraction of the cargo weight allocated to that station. Items shorter than 0.01 m are skipped to avoid divide-by-near-zero.

!!! note "Unplaced cargo is excluded"
    The sentinel `pos_x < 0` marks items that the bin-packing algorithm could not place. Unplaced items contribute nothing to the weight distribution, and therefore nothing to the strength calculation. A loading plan with many unplaced items will show an optimistically low SWBM.

## Distributing buoyancy

`distribute_buoyancy` starts from the simplest assumption for a box hull: buoyancy is uniform along the length. It then applies a 50% taper at the extreme bow and stern (within 5% of ship length from each end) to reflect that hull sections narrow sharply there:

```c
/* from src/longitudinal_strength.c */
if (frac < 0.05f) {
    taper = 0.5f + (frac / 0.05f) * 0.5f;
} else if (frac > 0.95f) {
    taper = 0.5f + ((1.0f - frac) / 0.05f) * 0.5f;
}
dist[i] *= taper;
```

After the taper the entire buoyancy array is scaled so its trapezoidal integral equals `displacement_t`. Equilibrium requires total weight = total buoyancy, and the normalisation enforces this.

## Net load and the two integrations

With both distributions filled, the net load at each station is straightforward:

```c
/* from src/longitudinal_strength.c */
r.net_load[i] = r.weight_dist[i] - r.buoyancy_dist[i];
```

Positive net load means excess weight (a downward tendency); negative means excess buoyancy (an upward tendency).

**Shear force** is then integrated forward from AP using the trapezoidal rule:

```c
/* from src/longitudinal_strength.c */
r.shear_force[0] = 0.0f;
for (int i = 1; i < LS_NUM_STATIONS; i++) {
    r.shear_force[i] = r.shear_force[i - 1] +
        (r.net_load[i - 1] + r.net_load[i]) * 0.5f * dx;
}
```

**Bending moment** is integrated from the already-computed shear force, again with the trapezoidal rule:

```c
/* from src/longitudinal_strength.c */
r.bending_moment[0] = 0.0f;
for (int i = 1; i < LS_NUM_STATIONS; i++) {
    r.bending_moment[i] = r.bending_moment[i - 1] +
        (r.shear_force[i - 1] + r.shear_force[i]) * 0.5f * dx;
}
```

Both arrays start at zero (free end at AP) and, if the weight-buoyancy balance is correct, should return to near-zero at FP.

!!! tip "Why integrate twice?"
    The double integration is not a numerical trick — it reflects the physics exactly. Shear force is the cumulative net load (units: tonnes). Bending moment is the cumulative lever arm of that net load (units: tonne-metres). Each integration adds one dimension of length, which is why the units change.

## Hogging versus sagging

The code identifies peak values across all stations:

```c
/* from src/longitudinal_strength.c */
if (r.bending_moment[i] > r.max_bm_hog) {
    r.max_bm_hog = r.bending_moment[i];
}
if (r.bending_moment[i] < 0 && fabsf(r.bending_moment[i]) > r.max_bm_sag) {
    r.max_bm_sag = fabsf(r.bending_moment[i]);
}
```

A **positive** bending moment means the hull arches upward amidships (hogging) — the deck is in tension and the keel is in compression. A **negative** bending moment means the ends are pushed up (sagging) — the keel is in tension. Class societies specify separate limits for each mode because the hull scantlings may be stronger in one direction than the other.

## Checking permissible limits

`check_strength_limits` in `longitudinal_strength.c` performs three comparisons against the `StrengthLimits` struct loaded from the ship config:

```c
/* from src/longitudinal_strength.c */
int check_strength_limits(const LongStrengthResult *result,
                          const StrengthLimits *limits) {
    if (!result || !limits) return -1;

    if (result->max_shear_force > limits->permissible_sf)
        return 0;
    if (result->max_bm_hog > limits->permissible_bm_hog)
        return 0;
    if (result->max_bm_sag > limits->permissible_bm_sag)
        return 0;

    return 1;
}
```

Return values: `1` = all limits satisfied, `0` = at least one exceeded, `-1` = not checked (NULL pointer). The `AnalysisResult` field `strength_compliant` carries one of these three values. When `strength_limits` is `NULL` in the `Ship` struct — because the ship config did not supply `permissible_sf_tonnes`, `permissible_bm_hog_t_m`, and `permissible_bm_sag_t_m` — the check is skipped and `strength_compliant` remains `-1`.

The three config keys and what they map to:

| Config key | Struct field | Unit |
|---|---|---|
| `permissible_sf_tonnes` | `StrengthLimits.permissible_sf` | tonnes |
| `permissible_bm_hog_t_m` | `StrengthLimits.permissible_bm_hog` | tonne-metres |
| `permissible_bm_sag_t_m` | `StrengthLimits.permissible_bm_sag` | tonne-metres |

!!! warning "Box-hull simplification"
    The entire longitudinal-strength module uses box-hull approximations for both weight and buoyancy distribution. Real class-society calculations (DNV, Lloyd's, BV) use hull-form-specific weight curves from the ship's light-weight survey and hydrostatic tables station by station. CargoForge-C's implementation is suitable for early-stage cargo planning and educational purposes; it should not be used as a substitute for the approved loading computer on an actual vessel.

## Where this fits in `perform_analysis`

`calculate_longitudinal_strength` is called from `analysis.c`'s `perform_analysis` after draft and displacement have been computed. Its results flow into `AnalysisResult`:

- `max_shear_force` — peak SWSF in tonnes.
- `max_bending_moment` — larger of `max_bm_hog` and `max_bm_sag` in tonne-metres.
- `strength_compliant` — the return value of `check_strength_limits`.

The JSON output layer (in `json_output.c`) serialises all three alongside the stability results, so a downstream tool or dashboard sees both intact stability and longitudinal strength in a single response.

## Recap

- CargoForge-C divides the hull into `LS_NUM_STATIONS = 20` evenly-spaced stations running from AP to FP.
- Weight is built from two contributions: a trapezoidal lightship profile (heavier amidships) plus each placed cargo item spread uniformly over its longitudinal footprint.
- Buoyancy is uniform with a 50% taper at bow and stern, then normalised to equal total displacement.
- Net load = weight − buoyancy at each station; integrating once gives shear force, integrating again gives bending moment.
- Positive bending moment = hogging; negative = sagging. Both are tracked separately against class-society limits.
- `check_strength_limits` returns `1` (pass), `0` (fail), or `−1` (not checked) depending on whether `StrengthLimits` was loaded from the ship config.

## Check yourself

??? question "What's the difference between hogging and sagging?"
    Hogging is the hull bending concave-down — midship pushed *up* relative to the ends, like a hog's back. Sagging is the opposite: concave-up, with midship pushed *down* relative to the ends.

??? question "Why can a stow that passes every GM and stability check still fail the longitudinal-strength check?"
    Stability is about the ship staying upright. Longitudinal strength is about the hull girder not physically bending or breaking under an uneven weight-vs-buoyancy distribution — a perfectly upright, perfectly balanced ship can still concentrate too much weight in one section and overstress the steel there.

*Next: [Lab 9 - Turn on a hydro table and a tank](lab-09-engine.md).*
