# The hydrostatic table

A ship is not a rectangular box, and treating it as one introduces errors that grow quickly as loading changes. This lesson introduces the hydrostatic table — a pre-computed dataset from a ship's stability booklet — and shows exactly how CargoForge-C reads, validates, and interpolates it to replace the box-hull fallback with real naval-architecture data.

---

## Why the box-hull model is only an approximation

When no table is available, `perform_analysis` in `analysis.c` falls back to a simplified box hull:

```
draft = displaced_volume / (L × B × C_b)       where C_b = 0.75
KB    = 0.53 × draft
BM    = (L × B³ / 12 × C_wp) / displaced_volume   where C_wp = 0.85
```

These constants — 0.75 for the block coefficient, 0.53 for the KB fraction, 0.85 for the waterplane coefficient — are reasonable ballpark figures for a generic cargo ship at moderate loading. But a real vessel's underwater form changes shape as it sinks deeper into the water. At light displacement the waterplane area may be quite different from what it is at full load; the centre of buoyancy shifts non-linearly; and the metacentric radius BM depends on the actual second moment of waterplane area, not a global constant.

A hydrostatic table records all of these values at a series of evenly-spaced drafts, calculated once (by the shipyard's naval architects) from the actual hull geometry. For any intermediate draft, the program interpolates. The result can be accurate to within fractions of a centimetre — the same figures a chief officer would read from the paper stability booklet on the bridge.

---

## What the CSV columns mean

The file `examples/sample_hydro.csv` ships with CargoForge-C as a worked example for a 150 m × 25 m general cargo vessel (block coefficient 0.75):

```csv
# Hydrostatic table for MV Example (150m x 25m general cargo vessel)
# Derived from typical cargo ship characteristics with Cb=0.75
# draft_m,displacement_t,KM_m,KB_m,BM_m,TPC_t_cm,MTC_t_m,waterplane_m2,LCB_m
2.0,2306,13.20,1.06,12.14,9.60,185.0,3375,1.5
3.0,3544,9.92,1.58,8.34,10.20,195.0,3570,1.2
...
10.0,13502,6.10,5.14,0.96,12.25,265.0,4070,-0.4
```

Each row is one draught. The nine columns are:

| Column | Symbol | Unit | What it describes |
|--------|--------|------|-------------------|
| `draft_m` | T | m | Mean draught from keel |
| `displacement_t` | Δ | tonnes | Total mass of water displaced |
| `KM_m` | KM | m | Height of transverse metacentre above keel |
| `KB_m` | KB | m | Height of centre of buoyancy above keel |
| `BM_m` | BM | m | Transverse metacentric radius (KM − KB) |
| `TPC_t_cm` | TPC | t/cm | Tonnes per centimetre immersion |
| `MTC_t_m` | MTC | t·m/cm | Moment to change trim 1 cm |
| `waterplane_m2` | $A_w$ | m² | Waterplane area at this draft |
| `LCB_m` | LCB | m | Longitudinal centre of buoyancy, measured from midship |

The relationship $KM = KB + BM$ holds at every row — KM is simply the combined column, provided for quick manual lookup. BM is the structurally important value: it measures how wide the waterplane is relative to the submerged volume:

$$BM = \frac{I_T}{\nabla}$$

where $I_T$ is the second moment of the waterplane area about the ship's centreline and $\nabla$ is displaced volume. A wide ship with a small displacement (shallow draft) has a large BM; as the ship settles deeper, BM falls — which is exactly what the table shows: BM drops from 12.14 m at 2 m draft to 0.96 m at 10 m draft.

!!! note "KM and GM are different things"
    The table gives KM — the height of the metacentre above the keel, a purely geometric property of the hull. GM (metacentric height) subtracts the cargo's centre of gravity: $GM = KM - KG$. KM comes from the table; KG comes from the loading calculation. The program computes both.

---

## TPC and MTC — what they are for

**TPC (tonnes per centimetre)** tells you how many tonnes of cargo must be added or removed to change the mean draught by exactly 1 cm. It is the derivative of displacement with respect to draft, scaled to convenient units. At 5 m draft the sample vessel shows TPC = 11.10, meaning adding 11.1 t sinks the ship 1 cm further.

**MTC (moment to change trim 1 cm)** is the longitudinal equivalent: the couple (in tonne-metres) required to rotate the vessel so that the difference between aft and forward drafts changes by 1 cm. CargoForge-C uses MTC to compute the longitudinal metacentric radius when table data is present:

$$BM_L = \frac{MTC \times 100 \times L}{\Delta}$$

and from that derives trim:

$$\text{trim} = \frac{LCG \times L}{GM_L} \quad \text{(metres by stern)}$$

where $LCG$ is the longitudinal distance between the centre of gravity and the centre of buoyancy.

---

## Reading and validating the table in C

`parse_hydro_table` in `src/hydrostatics.c` reads the CSV one line at a time, skipping `#` comments and blank lines:

```c
int parsed = sscanf(line, "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                    &e->draft, &e->displacement, &e->km, &e->kb,
                    &e->bm, &e->tpc, &e->mtc, &e->waterplane_area,
                    &e->lcb);

if (parsed < 7) {
    fprintf(stderr, "Warning: Skipping malformed hydrostatic entry at line %d "
            "(got %d fields, need at least 7)\n", line_num, parsed);
    continue;
}

/* Default LCB to 0 if not provided (8-column format) */
if (parsed < 9) e->lcb = 0.0f;
/* Default waterplane_area to 0 if not provided (7-column format) */
if (parsed < 8) e->waterplane_area = 0.0f;
```

Three format variants are accepted: 7 columns (no waterplane area or LCB), 8 columns (waterplane area but no LCB), and 9 columns (full). Missing trailing fields default to zero rather than causing a parse error — common in real stability booklets that predate standardised electronic formats.

After each row, the parser enforces strictly ascending draft order:

```c
if (table->count > 0 &&
    e->draft <= table->entries[table->count - 1].draft) {
    fprintf(stderr, "Error: Hydrostatic table not in ascending draft order "
            "at line %d (%.2f <= %.2f)\n",
            line_num, e->draft, table->entries[table->count - 1].draft);
    fclose(fp);
    return -1;
}
```

This requirement is not cosmetic: the interpolation functions both rely on the assumption that displacement increases monotonically with draft. A duplicate or reversed row would cause the binary search to silently return a wrong answer.

Finally, at least two entries are required, because a single data point cannot support interpolation:

```c
if (table->count < 2) {
    fprintf(stderr, "Error: Hydrostatic table needs at least 2 entries (got %d)\n",
            table->count);
    return -1;
}
```

---

## Interpolation: from displacement to all other quantities

After loading, the program uses the table in two directions.

### Forward: draft → everything else

`hydro_interpolate` takes a known draft and returns a fully populated `HydroEntry`. It finds the two bounding rows and interpolates linearly across all nine fields simultaneously:

```c
float range = hi->draft - lo->draft;
float t = (range > 1e-6f) ? (draft - lo->draft) / range : 0.0f;
interpolate_entries(lo, hi, t, result);
```

`interpolate_entries` calls `lerp` on every field:

```c
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) {
    out->draft        = lerp(lo->draft,        hi->draft,        t);
    out->displacement = lerp(lo->displacement, hi->displacement, t);
    out->km           = lerp(lo->km,           hi->km,           t);
    out->kb           = lerp(lo->kb,           hi->kb,           t);
    out->bm           = lerp(lo->bm,           hi->bm,           t);
    out->tpc          = lerp(lo->tpc,          hi->tpc,          t);
    out->mtc          = lerp(lo->mtc,          hi->mtc,          t);
    out->waterplane_area = lerp(lo->waterplane_area, hi->waterplane_area, t);
    out->lcb          = lerp(lo->lcb,          hi->lcb,          t);
}
```

If the requested draft falls outside the table range, the function clamps to the nearest boundary entry rather than extrapolating. Extrapolation beyond the measured range of a hydrostatic table is unreliable — hull form near the keel and near the freeboard deck deviates significantly from the middle-body behaviour that linear interpolation assumes.

### Inverse: displacement → draft

The more common computation direction is the reverse: the program knows displacement (lightship + cargo + tank liquid) and needs to find what draft results. `hydro_draft_from_displacement` does this by searching for the bracketing entries by displacement and inverting the interpolation:

```c
float range = disp_hi - disp_lo;
float t = (range > 1e-6f) ? (displacement_t - disp_lo) / range : 0.0f;
return lerp(table->entries[i].draft, table->entries[i + 1].draft, t);
```

Once draft is found, `hydro_interpolate` is called on that draft to populate KB, BM, MTC, and all other quantities for the stability calculation.

---

## How the table changes the analysis

When `ship->hydro->loaded == 1`, `perform_analysis` in `analysis.c` replaces every box-hull formula with a table lookup:

| Quantity | Box-hull formula | Table-based source |
|----------|------------------|--------------------|
| Draft | $\nabla / (L \times B \times 0.75)$ | `hydro_draft_from_displacement` |
| KB | $0.53 \times T$ | `HydroEntry.kb` |
| BM | $I_T / \nabla$ (approximate $I_T$) | `HydroEntry.bm` |
| $BM_L$ | Approximated from hull dimensions | Derived from `HydroEntry.mtc` |

The result field `hydro_table_used` in `AnalysisResult` is set to 1 when the table path was taken, and 0 when the box-hull fallback was used. You can check this in the JSON output or the human-readable report to confirm which model produced the numbers.

!!! warning "Overloading past the table boundary"
    If the computed displacement exceeds the heaviest entry in the table, `hydro_draft_from_displacement` clamps to the last row. The analysis will still complete, but the draft and all derived values are wrong. A separate check — comparing `total_cargo_weight_kg + lightship_weight` against `ship->max_weight` — rejects the load before the hydrostatic lookup is even attempted.

---

## Connecting the table to your ship config

To use a hydrostatic table, add one line to your ship configuration file:

```
hydrostatic_table = data/mv_example_hydro.csv
```

`parse_ship_config` in `parser.c` reads this key, stores the path, and immediately calls `parse_hydro_table`. If parsing fails, the ship config load returns `-1` and the program reports the error before attempting any analysis.

The tank configuration key (`tank_config`) works identically: one key in the config triggers the full CSV parse for free-surface correction data. Both paths must succeed for the analysis to proceed with full fidelity.

---

## Recap

- The hydrostatic table encodes how a specific hull's geometry — displacement, KB, BM, TPC, MTC, waterplane area, and LCB — varies with draft, pre-computed by naval architects from the actual hull lines.
- The nine-column CSV format used by CargoForge-C mirrors what real stability booklets provide; 7- and 8-column variants are also accepted with sensible defaults for missing fields.
- `parse_hydro_table` in `src/hydrostatics.c` enforces strictly ascending draft order because the interpolation functions require monotonicity.
- `hydro_draft_from_displacement` performs inverse interpolation: given a displacement (tonnes), it returns the draft — the direction the analysis always runs in practice.
- When a table is loaded, every approximate box-hull constant (0.75, 0.53, 0.85) is replaced by real data read from the table, and `AnalysisResult.hydro_table_used` is set to 1.
- Clamping at table boundaries prevents extrapolation; an overweight ship is caught earlier by the weight limit check.

*Next: [Lab 5 — Compute a Draft by Hand](lab-05-flotation.md).*
