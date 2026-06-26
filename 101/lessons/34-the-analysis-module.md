# The analysis module

`src/analysis.c` is the mathematical heart of CargoForge-C. It takes a fully populated `Ship` struct — loaded cargo, tanks, hull data — and produces a single `AnalysisResult` that answers the question every naval architect asks: *is this loading condition safe?* Understanding how `perform_analysis` is structured teaches you both the naval architecture and how a real C program organises a multi-step computation into one clean function.

---

## The conductor function

`perform_analysis` is 153 lines long and returns an `AnalysisResult` by value. It never modifies the ship; it only reads it. The signature says all of this in one line:

```c
// from src/analysis.c
AnalysisResult perform_analysis(const Ship *ship)
```

The `const Ship *` guarantees the function is a pure computation: given the same ship, it will always produce the same result. The return-by-value means the caller gets a complete snapshot it owns outright — no heap allocation, no pointer to manage.

Inside, the function follows a strict sequence. Each step feeds the next:

1. Single-pass cargo accumulation — weights and moments
2. Overweight guard
3. Hydrostatics — draft, KB, BM
4. KG (vertical centre of gravity)
5. GM before free-surface correction
6. Free-surface correction → corrected GM
7. Trim
8. Heel
9. GZ curve — wall-sided formula
10. IMO intact stability verdict
11. Longitudinal strength (optional)

Each phase is clearly marked by a comment block in the source. Working through them in order is the clearest way to understand both the code and the physics.

---

## Phase 1 — Cargo accumulation

The function opens with a single loop over `ship->cargo[]`:

```c
// from src/analysis.c:96-117
float moment_x = 0.0f;
float moment_y = 0.0f;
float vertical_moment = ship->lightship_weight * ship->lightship_kg;
float lcg_moment = 0.0f;

for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *c = &ship->cargo[i];
    if (c->pos_x < 0) continue;    /* skip unplaced cargo */

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

Several things to notice:

- **`pos_x < 0` is the sentinel.** The placement module sets `pos_x = pos_y = pos_z = -1.0f` for any item it could not fit. Analysis silently ignores those items. Only cargo that was actually placed counts toward stability.
- **Centre of the item, not the corner.** Positions in the struct are the low corner of the bounding box. The moments use `pos_x + dimensions[0]/2` — the geometric centre.
- **`vertical_moment` starts with lightship.** Before any cargo is seen, the accumulator already holds the lightship's contribution. This avoids a separate pass and a potential off-by-one.
- **`lcg_moment` measures from midship.** The longitudinal CG offset from the midship point ($L/2$) determines trim direction and magnitude.

If tanks are loaded, their weight and vertical moment are added immediately after the cargo loop, folded into the same running totals.

---

## Phase 2 — Overweight guard

```c
// from src/analysis.c:130-133
if (displacement_kg > ship->max_weight) {
    r.gm = NAN;
    return r;
}
```

This is the earliest possible rejection. If the total displacement exceeds the ship's class limit, no stability calculation is meaningful. The sentinel value `NAN` (Not a Number) in `r.gm` propagates through any downstream code that checks it — `isnan(r.gm)` returns true, so `print_loading_plan` prints `PLAN REJECTED` instead of a stability table, and `json_output.c` emits `null` for all numeric fields.

Using `NAN` as a sentinel is a deliberate choice: it is a value that cannot arise from valid arithmetic, so it cannot be confused with a real GM of zero.

---

## Phase 3 — Hydrostatics: draft, KB, BM

The function must find the ship's draft before it can compute the metacentric height. There are two paths, selected by whether a hydrostatic table was loaded:

```c
// from src/analysis.c:148-182
if (ship->hydro && ship->hydro->loaded) {
    r.hydro_table_used = 1;
    r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);
    HydroEntry he;
    hydro_interpolate(ship->hydro, r.draft, &he);
    r.kb = he.kb;
    r.bm = he.bm;
    /* ... bm_l from MTC ... */
} else {
    r.hydro_table_used = 0;
    r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
    r.kb = KB_FACTOR * r.draft;
    float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
    r.bm = inertia_t / displaced_vol;
    /* ... */
}
```

**Box-hull fallback.** When no table is available, the module treats the hull as a rectangular box with two empirical coefficients:

| Constant | Value | Meaning |
|---|---|---|
| `BLOCK_COEFF` | 0.75 | Fraction of L×B×T that the hull actually displaces |
| `WATERPLANE_COEFF` | 0.85 | Fraction of L×B that is waterplane area |
| `KB_FACTOR` | 0.53 | KB ≈ 53% of draft for typical cargo ships |

These give reasonable approximations for initial planning but are no substitute for real hull data.

**Table-based path.** When a CSV table is loaded, `hydro_draft_from_displacement` does an inverse interpolation — it finds the draft that corresponds to the computed displacement. Then `hydro_interpolate` reads KB and BM directly from the table at that draft. Real hull geometry, measured and tabulated, replaces the box assumptions.

---

## Phase 4 — KG and GM

KG (the ship's vertical centre of gravity) and GM (the metacentric height, the primary measure of initial stability) follow directly from the accumulated moments:

$$KG = \frac{\text{vertical moment}}{\text{displacement}}$$

$$GM = KB + BM - KG$$

In the code:

```c
// from src/analysis.c:146, 185
r.kg = vertical_moment / displacement_kg;
r.gm = r.kb + r.bm - r.kg;
```

This is the fundamental stability triangle. K is the keel, B is the centre of buoyancy, M is the transverse metacentre. A positive GM means the ship will return upright when heeled; a negative GM means it will capsize.

!!! note "Why KG appears in both numerator and denominator"
    `vertical_moment` was accumulated in kg·m (mass × height). Dividing by `displacement_kg` gives metres. The result is the height of the overall centre of gravity above the keel — the same unit as KB and BM, so the subtraction $KB + BM - KG$ is dimensionally consistent.

---

## Phase 5 — Free-surface correction

Partially-filled tanks behave like a liquid pendulum: when the ship heels, the liquid shifts, raising the effective centre of gravity. This virtual rise in KG reduces GM without adding any weight.

```c
// from src/analysis.c:188-192
r.free_surface_correction = 0.0f;
if (ship->tanks && ship->tanks->count > 0) {
    r.free_surface_correction = calculate_virtual_kg_rise(ship->tanks, displacement_t);
}
r.gm_corrected = r.gm - r.free_surface_correction;
```

`calculate_virtual_kg_rise` (in `tanks.c`) sums the free-surface moment of each partially-filled rectangular tank:

$$\text{FSM} = \rho \cdot \frac{l \cdot b^3}{12}$$

The breadth $b$ appears cubed because a wider tank shifts more liquid per degree of heel. Tanks that are empty or 100% full contribute zero — there is no free surface to move.

The corrected GM is used for everything that follows. All IMO criteria, heel calculations, and GZ curve values use `gm_corrected`, never the raw `gm`.

---

## Phase 6 — Trim and heel

**Trim** is how much the bow or stern sits deeper in the water. It is driven by the longitudinal distance between the overall centre of gravity and the centre of buoyancy:

```c
// from src/analysis.c:198-202
r.lcg = lcg_moment / displacement_kg;
float gm_l = r.kb + bm_l - r.kg;
if (gm_l > 0.01f)
    r.trim = r.lcg * ship->length / gm_l;
```

$GM_L$ is the *longitudinal* metacentric height — analogous to $GM$ but for pitching rather than rolling. A positive `r.lcg` means the cargo centre of gravity is aft of midship, so the ship trims by the stern.

**Heel** is a sideways tilt caused by the transverse cargo imbalance:

```c
// from src/analysis.c:204-211
float avg_y = moment_y / r.total_cargo_weight_kg;
float tcg = avg_y - ship->width / 2.0f;
if (gm_effective > 0.01f)
    r.heel = atanf(tcg / gm_effective) * 180.0f / (float)M_PI;
```

`tcg` (transverse centre of gravity offset from centreline) divided by $GM$ gives the tangent of the heel angle. The guard `gm_effective > 0.01f` prevents division by near-zero — a ship with zero GM would theoretically heel to 90°, which is beyond the model's valid range.

---

## Phase 7 — The GZ curve

GZ is the *righting lever*: the horizontal distance between the vertical through the centre of gravity and the vertical through the centre of buoyancy when the ship is heeled to angle $\theta$. A large GZ means the ship is strongly self-righting at that angle.

CargoForge-C uses the **wall-sided formula**, a standard naval-architecture approximation for moderate angles:

$$GZ(\theta) = \sin\theta \left[ GM + \frac{BM \cdot \tan^2\theta}{2} \right]$$

The implementation in `gz_at_angle`:

```c
// from src/analysis.c:45-49
static float gz_at_angle(float gm, float bm, float theta_deg) {
    float theta = theta_deg * (float)M_PI / 180.0f;
    float tan_theta = tanf(theta);
    return sinf(theta) * (gm + bm * tan_theta * tan_theta / 2.0f);
}
```

!!! warning "Valid range"
    The wall-sided formula is reliable up to roughly 40°. Beyond that, hull geometry (bilge rounding, flare, freeboard) matters more than the formula captures. CargoForge-C scans to 80° for the maximum GZ search but the IMO criteria themselves focus on 30–40°, well within the valid range.

Areas under the GZ curve are integrated with the trapezoidal rule over 100 steps:

```c
// from src/analysis.c:55-72 (abridged)
static float integrate_gz(float gm, float bm, float start_deg, float end_deg) {
    int steps = 100;
    float step = (end_deg - start_deg) / steps;
    float area = 0.0f;
    for (int i = 0; i < steps; i++) {
        float a1 = start_deg + i * step;
        float a2 = a1 + step;
        float gz1 = gz_at_angle(gm, bm, a1);
        float gz2 = gz_at_angle(gm, bm, a2);
        if (gz1 < 0) gz1 = 0;   /* only restoring moment counts */
        if (gz2 < 0) gz2 = 0;
        float da = step * (float)M_PI / 180.0f;
        area += (gz1 + gz2) * 0.5f * da;
    }
    return area;
}
```

The angle step is converted to radians before accumulation because GZ area has units of metre·radian (m·rad), as required by the IMO standard.

---

## Phase 8 — The IMO verdict

Six numerical thresholds are checked in a single compound expression:

```c
// from src/analysis.c:221-228
r.imo_compliant = (
    gm_effective   >= IMO_GM_MIN       &&   /* 0.15 m */
    r.gz_at_30     >= IMO_GZ_AT_30_MIN &&   /* 0.20 m */
    r.gz_max_angle >= IMO_GZ_MAX_ANGLE &&   /* 25 deg */
    r.area_0_30    >= IMO_AREA_0_30_MIN &&  /* 0.055 m·rad */
    r.area_0_40    >= IMO_AREA_0_40_MIN &&  /* 0.090 m·rad */
    r.area_30_40   >= IMO_AREA_30_40_MIN    /* 0.030 m·rad */
) ? 1 : 0;
```

These constants are defined at the top of `analysis.c` (lines 27–32) with inline references to the IMO instrument (MSC.267(85), Part A, Chapter 2.2). Every threshold has a physical justification — the area criteria ensure the ship has enough *reserve of stability* to survive a wave impact, not merely that it is initially upright.

`imo_compliant = 1` only when **all six** pass. A ship that passes five out of six is still non-compliant.

---

## Phase 9 — Longitudinal strength (optional)

If the ship configuration included permissible limits, the final phase calls into `longitudinal_strength.c`:

```c
// from src/analysis.c:231-239
if (ship->strength_limits) {
    LongStrengthResult ls = calculate_longitudinal_strength(
        ship, r.draft, displacement_t, ship->length, ship->width);
    r.max_shear_force = ls.max_shear_force;
    r.max_bending_moment = (ls.max_bm_hog > ls.max_bm_sag)
                           ? ls.max_bm_hog : ls.max_bm_sag;
    r.strength_compliant = check_strength_limits(&ls, ship->strength_limits);
}
```

When `ship->strength_limits` is NULL, this block is skipped entirely and `r.strength_compliant` remains `-1` — the sentinel meaning "not checked". The caller can inspect this flag to know whether a strength verdict is available. This opt-in design means strength checking costs nothing unless the ship config provides the necessary limits.

---

## How the pieces fit together

```
parse_ship_config()        parse_cargo_list()
        │                         │
        └─────── Ship ────────────┘
                   │
           perform_analysis()
                   │
       ┌───────────┼───────────────┐
  cargo loop    hydrostatics    tanks FSC
       │              │              │
   weights +      draft, KB,     free surface
   moments        BM (table      correction
                  or box)        │
                  │              └──> gm_corrected
                  │                       │
               KG, GM               trim, heel
                                          │
                                     GZ curve
                                          │
                                    IMO verdict
                                          │
                              (optional) SWSF/SWBM
                                          │
                                   AnalysisResult
```

`perform_analysis` is a pure function: same input, same output. It never touches heap memory or global state. Callers — `print_loading_plan`, `cargoforge_analyze` in the library, the JSON and CSV output paths — all call it independently and receive their own copy of the result.

---

## Recap

- `perform_analysis` is a single-pass conductor: cargo loop → hydrostatics → KG/GM → free-surface → trim/heel → GZ → IMO verdict → strength.
- Unplaced cargo (sentinel `pos_x < 0`) is silently excluded; overweight ships return early with `r.gm = NAN`.
- Two hydrostatic paths exist: real table interpolation when a CSV is loaded, box-hull approximation otherwise.
- The free-surface correction reduces the effective GM by the virtual KG rise from partially-filled tanks; all stability criteria use this corrected value.
- Six IMO criteria from MSC.267(85) are checked together; all must pass for `imo_compliant = 1`.
- Longitudinal strength is opt-in: only evaluated when `ship->strength_limits` is non-NULL.

*Next: [Hydrostatic interpolation](35-hydrostatic-interpolation.md).*
