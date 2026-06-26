# The center of buoyancy and the metacenter

A ship floats because the water it displaces pushes back. Where exactly that push acts — and how it shifts when the ship tilts — determines whether the vessel rights itself or capsizes. This lesson explains the geometry behind buoyancy: KB (the vertical center of buoyancy), BM (the metacentric radius), and the metacenter M — the pivot point that [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) computes before every stability check.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **KB (vertical centre of buoyancy)** = "how high up the ship's underwater push is acting" — `perform_analysis` estimates this as `KB_FACTOR * draft` (0.53 × the depth underwater) for a typical cargo hull, or reads it from a hydrostatic table when one is loaded.
- **BM (metacentric radius)** = "how much the ship's stability cushion grows with beam" — computed in `analysis.c` as waterplane inertia divided by displaced volume (`inertia_t / displaced_vol`); because beam is cubed in the formula, a wider ship gets a dramatically bigger BM for free.
- **M (metacenter)** = "the pivot point the ship rocks around for small rolls" — it is not a physical fitting on the vessel, just a geometric point; `perform_analysis` locates it with the single line `r.gm = r.kb + r.bm - r.kg`.
- **GM** = "the safety margin between where the ship rocks and where its weight sits" — positive means the ship pushes back upright; negative means it will capsize; this number is what every call to `perform_analysis` ultimately produces.
- **Hydrostatic table vs. box-hull fallback** = "measured hull data vs. a good approximation" — when `ship->hydro->loaded == 1`, `hydrostatics.c` does a two-step lookup (displacement → draft via `hydro_draft_from_displacement`, then draft → KB/BM via `hydro_interpolate`); otherwise fixed coefficients (`KB_FACTOR = 0.53`, `WATERPLANE_COEFF = 0.85`) stand in.
- **Waterplane second moment of area (I_T)** = "how spread-out the ship's footprint at the waterline is" — the wider the ship, the larger I_T, the larger BM, and the more resistant it is to rolling; this is why cargo ships are built wide rather than tall.

**Why it matters:** if KB or BM is wrong, GM is wrong, and a ship declared stable may capsize at sea; getting the two-mode calculation (`hydro_table_used = 0` vs `1`) correct is the difference between a planning estimate and a result fit for classification-society submission.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> A see-saw bolted to the ceiling. The bolt is M (the metacenter). Your cargo is the weight on one end (G). As long as the bolt is *above* the weight's center, the see-saw swings back to level on its own. Drop the bolt *below* the weight — it tips and never stops.

That bolt-position is what `r.gm = r.kb + r.bm - r.kg` computes. `r.kb + r.bm` locates M above the keel; `r.kg` is how high the cargo pile sits. The difference is GM — the gap between the ceiling bolt and the weight. When `perform_analysis` fills in those three floats, it is asking: *is the bolt still above the weight after we loaded this cargo?*

The beam-cubed term in BM (`powf(ship->width, 3) / 12.0f`) is what keeps the bolt high. Make the ship wider and M rises fast — which is exactly why cargo ships look squat rather than tall.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Ship cross-section: K, B, G, and M on the centreline</title>
  <desc>A schematic hull cross-section showing the keel (K) at the bottom, the centre of buoyancy (B) mid-draft, the centre of gravity (G) above B, and the metacenter (M) at the top. GM is positive because M is above G.</desc>

  <!-- hull outline -->
  <path d="M 180 240 Q 200 280 310 285 Q 420 280 440 240 L 430 160 L 190 160 Z"
        fill="none" stroke="currentColor" stroke-width="2.5" stroke-opacity="0.55"/>

  <!-- waterline -->
  <line x1="155" y1="185" x2="465" y2="185"
        stroke="#12A594" stroke-width="1.8" stroke-dasharray="8 4" opacity="0.85"/>
  <text x="468" y="189" font-size="12" fill="#12A594" opacity="0.9">WL</text>

  <!-- centreline -->
  <line x1="310" y1="50" x2="310" y2="290"
        stroke="currentColor" stroke-width="1" stroke-dasharray="4 3" opacity="0.3"/>

  <!-- K — keel -->
  <circle cx="310" cy="283" r="4" fill="#D05663"/>
  <text x="318" y="287" font-size="12.5" fill="#D05663" font-weight="600">K</text>
  <text x="330" y="287" font-size="11" fill="currentColor" opacity="0.6">(keel)</text>

  <!-- B — centre of buoyancy -->
  <circle cx="310" cy="234" r="5" fill="#12A594"/>
  <text x="318" y="238" font-size="12.5" fill="#12A594" font-weight="600">B</text>
  <text x="330" y="238" font-size="11" fill="currentColor" opacity="0.6">(r.kb = 0.53 × draft)</text>

  <!-- G — centre of gravity -->
  <circle cx="310" cy="175" r="5" fill="#D05663"/>
  <text x="318" y="179" font-size="12.5" fill="#D05663" font-weight="600">G</text>
  <text x="330" y="179" font-size="11" fill="currentColor" opacity="0.6">(r.kg — cargo height)</text>

  <!-- M — metacenter -->
  <circle cx="310" cy="108" r="6" fill="#12A594" fill-opacity="0.15" stroke="#12A594" stroke-width="2"/>
  <text x="318" y="113" font-size="12.5" fill="#12A594" font-weight="700">M</text>
  <text x="330" y="113" font-size="11" fill="currentColor" opacity="0.6">(metacenter = KB + BM)</text>

  <!-- BM bracket -->
  <line x1="155" y1="234" x2="155" y2="108"
        stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <line x1="150" y1="234" x2="160" y2="234" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <line x1="150" y1="108" x2="160" y2="108" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <text x="100" y="176" font-size="11.5" fill="currentColor" opacity="0.6" text-anchor="middle">BM</text>
  <text x="100" y="190" font-size="10" fill="currentColor" opacity="0.5" text-anchor="middle">I_T / V</text>

  <!-- KB bracket -->
  <line x1="465" y1="283" x2="465" y2="234"
        stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <line x1="460" y1="283" x2="470" y2="283" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <line x1="460" y1="234" x2="470" y2="234" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <text x="490" y="263" font-size="11.5" fill="currentColor" opacity="0.6">KB</text>

  <!-- GM bracket — teal = good distance -->
  <line x1="82" y1="175" x2="82" y2="108"
        stroke="#12A594" stroke-width="2" opacity="0.8"/>
  <line x1="77" y1="175" x2="87" y2="175" stroke="#12A594" stroke-width="1.5" opacity="0.8"/>
  <line x1="77" y1="108" x2="87" y2="108" stroke="#12A594" stroke-width="1.5" opacity="0.8"/>
  <text x="60" y="147" font-size="12" fill="#12A594" font-weight="700" text-anchor="middle">GM</text>
  <text x="60" y="161" font-size="10" fill="#12A594" opacity="0.75" text-anchor="middle">positive</text>

  <!-- formula label -->
  <text x="310" y="38" font-size="12" fill="currentColor" opacity="0.7" text-anchor="middle">r.gm = r.kb + r.bm − r.kg</text>
</svg>

## Why geometry matters

Place a brick in a bathtub. The water level rises. The weight of water displaced exactly equals the weight of the brick (Archimedes' principle). But a ship is not a brick — it is hollow and asymmetric, and it can roll. Stability asks: *when it rolls by a small angle, does the net force push it back upright, or push it further over?*

The answer depends on the relative positions of two points:

| Symbol | Name | What it is |
|--------|------|------------|
| G | Centre of gravity | Where the ship's total weight acts (downward) |
| B | Centre of buoyancy | Centroid of the displaced volume (upward thrust acts here) |
| M | Metacenter | The point B travels toward as the ship heels; stability pivot |

When M is above G, the ship is stable. When M is below G, it capsizes. The vertical distance $GM$ is the single most important number in stability analysis.

---

## KB — the vertical centre of buoyancy

B is the geometric centroid of the submerged volume. For a flat-bottomed box hull floating upright, B sits at exactly half the draft. Real hulls are wider in the middle and narrower at the keel, so B sits a little higher than the halfway point.

### The box-hull approximation

`analysis.c` defines a constant at the top of the file:

```c
// from src/analysis.c
#define KB_FACTOR  0.53f  /* KB ~ 0.53*T for typical cargo ships */
```

When no hydrostatic table is loaded, `perform_analysis` estimates KB as:

```c
// from src/analysis.c (lines 173-174)
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
r.kb = KB_FACTOR * r.draft;
```

The constant 0.53 encodes the fact that the centroid of a ship-shaped cross-section sits at about 53 % of the draft — slightly above the midpoint because the hull is fuller near the waterline than near the keel.

### Reading KB from a hydrostatic table

When a hydrostatic CSV is loaded (`ship->hydro->loaded == 1`), KB is measured for the actual hull at every draft and interpolated from the table:

```c
// from src/analysis.c (lines 152-157)
r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);

HydroEntry he;
hydro_interpolate(ship->hydro, r.draft, &he);

r.kb = he.kb;
r.bm = he.bm;
```

`hydro_interpolate` finds the two table rows that bracket the current draft and performs linear interpolation across all fields at once — including `kb` and `bm`:

```c
// from src/hydrostatics.c (lines 126-134)
for (int i = 0; i < table->count - 1; i++) {
    const HydroEntry *lo = &table->entries[i];
    const HydroEntry *hi = &table->entries[i + 1];

    if (draft >= lo->draft && draft <= hi->draft) {
        float range = hi->draft - lo->draft;
        float t = (range > 1e-6f) ? (draft - lo->draft) / range : 0.0f;
        interpolate_entries(lo, hi, t, result);
        return 0;
    }
}
```

The `interpolate_entries` helper applies `lerp` to every field of `HydroEntry`:

```c
// from src/hydrostatics.c (lines 24-35)
static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) {
    out->draft        = lerp(lo->draft,        hi->draft,        t);
    out->kb           = lerp(lo->kb,           hi->kb,           t);
    out->bm           = lerp(lo->bm,           hi->bm,           t);
    // ... and all other fields
}
```

!!! note "Why clamp at the table boundaries?"
    `hydro_interpolate` returns the nearest row when the requested draft falls outside the table range. Extrapolating hydrostatic curves beyond measured data introduces errors that can exceed 10 %. Clamping is the conservative choice — and the code is explicit about it (lines 116–123 of `hydrostatics.c`).

---

## BM — the metacentric radius

When a ship heels by a small angle $\theta$, the submerged volume shifts to the low side: more volume is submerged to starboard if the ship rolls to starboard. B moves with it. For small angles, B traces an arc whose radius of curvature is called BM:

$$BM = \frac{I_T}{V}$$

where:

- $I_T$ is the **second moment of area of the waterplane** about the ship's centreline (m⁴)
- $V$ is the **displaced volume** (m³)

### What is the waterplane second moment of area?

Imagine cutting the ship horizontally at the waterline. The resulting shape is the waterplane — roughly an oval. The second moment of area $I_T$ measures how that area is distributed away from the centreline:

$$I_T = \int y^2 \, dA$$

Wide beams give large $I_T$. For a perfect rectangle of length $L$ and beam $B$:

$$I_T = \frac{L B^3}{12}$$

The beam enters as a cube, so doubling the beam increases BM eightfold. This is why ships are wide, not tall.

### The box-hull computation in `analysis.c`

```c
// from src/analysis.c (lines 175-177)
/* BM: transverse metacentric radius = I_T / V */
float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
r.bm = inertia_t / displaced_vol;
```

`WATERPLANE_COEFF` (0.85) scales the rectangle down to account for the fact that real waterplanes are not perfect rectangles — they narrow at bow and stern:

```c
// from src/analysis.c (line 23)
#define WATERPLANE_COEFF  0.85f  /* Cw: waterplane area / L*B */
```

So the effective second moment is:

$$I_T \approx C_w \cdot \frac{L B^3}{12}$$

and therefore:

$$BM \approx \frac{C_w \cdot L B^3 / 12}{V}$$

When using a hydrostatic table, `bm` is read directly from the `HydroEntry` — the naval architect who computed the table already integrated the actual hull geometry precisely.

---

## M — the metacenter

M is defined as the point where the vertical line through B (the buoyancy force line) intersects the ship's centreline when the ship heels by a small angle. For small heels, M stays in one place — it is a property of the waterplane geometry and the displacement, not of the heel angle itself.

The height of M above the keel (KM) is simply:

$$KM = KB + BM$$

CargoForge-C stores KB and BM separately so that GM can be computed for any cargo configuration without re-measuring the hull geometry:

$$GM = KB + BM - KG$$

This is the one line that brings the whole lesson together, and it appears verbatim in `perform_analysis`:

```c
// from src/analysis.c (line 185)
r.gm = r.kb + r.bm - r.kg;
```

!!! warning "Positive GM is not the whole story"
    A positive GM means the ship will resist *small* heels. IMO regulations also require that GZ (the righting lever at large angles) stays large enough over 0–40°. The next lessons cover the GZ curve and the free-surface correction that reduces the effective GM when tanks are partially filled.

---

## From displacement to draft — inverse interpolation

Before KB and BM can be read from the table, the code must know the draft that corresponds to the current displacement. Displacement is known (it is the sum of all weights). Draft is not. `hydrostatics.c` solves this by inverse-interpolating on the displacement column:

```c
// from src/hydrostatics.c (lines 154-162)
for (int i = 0; i < table->count - 1; i++) {
    float disp_lo = table->entries[i].displacement;
    float disp_hi = table->entries[i + 1].displacement;

    if (displacement_t >= disp_lo && displacement_t <= disp_hi) {
        float range = disp_hi - disp_lo;
        float t = (range > 1e-6f) ? (displacement_t - disp_lo) / range : 0.0f;
        return lerp(table->entries[i].draft, table->entries[i + 1].draft, t);
    }
}
```

The fraction `t` locates the displacement between two table rows; the same fraction is applied to the draft column. Once the draft is known, `hydro_interpolate` is called to get KB, BM, and all other parameters at that draft.

!!! tip "Two-step lookup"
    The process is always: displacement → draft (inverse interpolation via `hydro_draft_from_displacement`) → KB and BM (forward interpolation via `hydro_interpolate`). The code in `perform_analysis` (lines 151–156 of `analysis.c`) follows this exact sequence.

---

## Two modes side by side

| | Box-hull fallback | Hydrostatic table |
|---|---|---|
| **Trigger** | `ship->hydro == NULL` or not loaded | `ship->hydro->loaded == 1` |
| **Draft** | $V / (L \cdot B \cdot C_b)$ | Inverse interpolation on displacement column |
| **KB** | $0.53 \times \text{draft}$ | Read from `HydroEntry.kb` |
| **BM** | $C_w \cdot L B^3 / (12 V)$ | Read from `HydroEntry.bm` |
| **Accuracy** | Adequate for early planning | Required for classification-society submission |
| **Reported as** | `hydro_table_used = 0` | `hydro_table_used = 1` |

The human-readable output from `print_loading_plan` reports which mode was used:

```
Hydrostatics: Table interpolation
```
or
```
Hydrostatics: Box-hull approximation
```

---

## Recap

- **B** is the centroid of the displaced volume; its vertical position KB is proportional to draft (box-hull) or read from a CSV table (table mode).
- **$BM = I_T / V$**: the waterplane second moment of area divided by displaced volume. Beam enters as a cube — wide ships are inherently more stable.
- **M** (the metacenter) is the pivot point for small heels. Its height above the keel is $KM = KB + BM$.
- **$GM = KB + BM - KG$**: positive means self-righting; computed in `perform_analysis` as `r.gm = r.kb + r.bm - r.kg`.
- When a hydrostatic table is loaded, `hydrostatics.c` performs a two-step lookup — inverse interpolation to find draft from displacement, then forward interpolation to read KB and BM at that draft.
- The box-hull fallback uses fixed coefficients (`KB_FACTOR = 0.53`, `WATERPLANE_COEFF = 0.85`) and is appropriate for design exploration, not final certification.

*Next: [GM: why ships self-right](23-gm-why-ships-self-right.md).*
