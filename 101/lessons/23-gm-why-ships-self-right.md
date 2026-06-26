# GM: why ships self-right

A ship tilted by a wave, a gust of wind, or a shifting load must develop a force that pushes it back upright. Whether that force exists — and how strong it is — reduces to a single number: the **metacentric height**, GM. This lesson unpacks what GM means physically, how the equation $GM = KB + BM - KG$ is derived from first principles, and how `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) computes and judges it.

---

## The three heights

Every stability calculation pivots on three vertical distances, all measured from the **keel** (the bottom of the hull) upward.

| Symbol | Name | What it marks |
|--------|------|---------------|
| $K$ | Keel | Reference origin — always 0 |
| $B$ | Centre of Buoyancy | Centroid of the **submerged volume**; pushes upward |
| $G$ | Centre of Gravity | Centroid of the **entire ship weight**; pulls downward |
| $M$ | Transverse Metacentre | The point about which the ship rotates at small angles |

So:

- $KB$ — how high the buoyancy force acts
- $KG$ — how high the weight force acts
- $KM = KB + BM$ — height of the metacentre above the keel

And:

$$GM = KB + BM - KG = KM - KG$$

<svg viewBox="0 0 620 372" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The metacentric diagram: a heeled ship rights itself when M is above G</title>
<desc>A ship heeled to one side. Weight acts down through the centre of gravity G; buoyancy acts up through the shifted centre of buoyancy B. The vertical through B meets the ship's centreline at the metacentre M. The horizontal gap between the two force lines is the righting arm GZ. Because M is above G, the couple rotates the ship back upright.</desc>
<defs>
<marker id="ar-up" viewBox="0 0 10 10" refX="5" refY="9" markerWidth="7" markerHeight="7" orient="auto"><path d="M5 0 L9 9 L1 9 Z" fill="#12A594"/></marker>
<marker id="ar-dn" viewBox="0 0 10 10" refX="5" refY="1" markerWidth="7" markerHeight="7" orient="auto"><path d="M5 10 L1 1 L9 1 Z" fill="#D05663"/></marker>
</defs>
<!-- waterline -->
<line x1="70" y1="222" x2="552" y2="222" stroke="currentColor" stroke-width="1" opacity="0.35"/>
<text x="552" y="218" fill="currentColor" font-size="11" opacity="0.5" text-anchor="end">waterline</text>
<!-- heeled hull -->
<path d="M232,104 L246,286 Q250,316 270,308 Q310,294 392,156" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-width="1.4" opacity="0.7"/>
<line x1="232" y1="104" x2="392" y2="156" stroke="currentColor" stroke-width="1.4" opacity="0.7"/>
<!-- ship centreline (tilted), K -> G -> M -->
<line x1="255" y1="309" x2="329" y2="81" stroke="currentColor" stroke-width="1" stroke-dasharray="4 3" opacity="0.55"/>
<!-- force line of buoyancy: world-vertical through B, up to M -->
<line x1="329" y1="250" x2="329" y2="110" stroke="#12A594" stroke-width="1.6" marker-end="url(#ar-up)"/>
<!-- force line of weight: world-vertical through G, downward -->
<line x1="289" y1="204" x2="289" y2="330" stroke="#D05663" stroke-width="1.6" marker-end="url(#ar-dn)"/>
<!-- righting arm GZ -->
<line x1="289" y1="204" x2="329" y2="204" stroke="#12A594" stroke-width="3"/>
<text x="309" y="196" fill="#12A594" font-size="13" font-weight="600" text-anchor="middle">GZ</text>
<!-- heel angle arc at M -->
<path d="M329,116 A35 35 0 0 0 314,123" fill="none" stroke="currentColor" stroke-width="1" opacity="0.55"/>
<text x="322" y="133" fill="currentColor" font-size="12" opacity="0.7">θ</text>
<!-- points -->
<circle cx="255" cy="309" r="3.4" fill="currentColor"/><text x="245" y="313" fill="currentColor" font-size="13" text-anchor="end" font-weight="600">K</text>
<circle cx="329" cy="250" r="3.4" fill="#12A594"/><text x="339" y="254" fill="#12A594" font-size="13" font-weight="600">B</text>
<circle cx="289" cy="204" r="3.4" fill="#D05663"/><text x="279" y="208" fill="#D05663" font-size="13" text-anchor="end" font-weight="600">G</text>
<circle cx="329" cy="81" r="3.4" fill="currentColor"/><text x="339" y="85" fill="currentColor" font-size="13" font-weight="600">M</text>
<!-- labels for the forces -->
<text x="329" y="104" fill="#12A594" font-size="11" text-anchor="middle">buoyancy ↑</text>
<text x="289" y="345" fill="#D05663" font-size="11" text-anchor="middle">weight ↓</text>
<!-- caption -->
<text x="430" y="250" fill="currentColor" font-size="12.5" opacity="0.75">M is above G,</text>
<text x="430" y="269" fill="currentColor" font-size="12.5" opacity="0.75">so the couple</text>
<text x="430" y="288" fill="#12A594" font-size="12.5" font-weight="600">rights the ship.</text>
<text x="430" y="312" fill="currentColor" font-size="11.5" opacity="0.6">GM = GZ / sin θ &gt; 0</text>
</svg>

---

## Why GM tells you whether the ship self-rights

Imagine tilting the ship slightly to starboard by angle $\theta$. The submerged volume shifts toward the low side, moving $B$ to starboard. The weight still acts through $G$ on the centreline. These two forces — buoyancy up through the new $B$, gravity down through $G$ — form a couple.

The horizontal separation between where those forces act is called the **righting lever** $GZ$:

$$GZ(\theta) \approx GM \cdot \sin\theta \quad \text{(for small angles)}$$

- If $GM > 0$: $G$ is **below** $M$. The couple acts to push the ship back upright. The ship is **stable**.
- If $GM = 0$: no restoring force at small angles. Neutral — marginally dangerous.
- If $GM < 0$: $G$ is **above** $M$. The couple acts to push the ship **further** over. **Capsizing**.

!!! warning "Negative GM"
    A ship with $GM < 0$ will not return to upright after a small disturbance. It will find a new equilibrium (the "loll angle") or capsize. Causes include top-heavy loading, free liquid in tanks, or ice accumulation on topsides.

---

## How the code computes it

`perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) works in two modes depending on whether a hydrostatic table CSV has been loaded.

### Box-hull fallback (no CSV table)

When `ship->hydro` is NULL, the code uses analytic formulas for a rectangular barge:

```c
/* from src/analysis.c — box-hull path */
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
r.kb = KB_FACTOR * r.draft;

float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
r.bm = inertia_t / displaced_vol;
```

The constants are defined at the top of the file:

```c
#define BLOCK_COEFF      0.75f   /* Cb: underwater hull vol / bounding box */
#define WATERPLANE_COEFF 0.85f   /* Cw: waterplane area / L*B              */
#define KB_FACTOR        0.53f   /* KB ~ 0.53*T for typical cargo ships     */
```

**Why these values?**

- `BLOCK_COEFF = 0.75` means the hull displaces 75% of its bounding box volume — typical for a loaded cargo vessel (a box barge = 1.0, a streamlined yacht ≈ 0.35).
- `KB_FACTOR = 0.53` is the empirical rule-of-thumb $KB \approx 0.53T$ valid for cargo-ship hull forms; the true value depends on the hull's underwater shape.
- `WATERPLANE_COEFF = 0.85` scales the waterplane area below a full rectangle, giving a more realistic second moment of area $I_T$.

$BM$ comes from the transverse second moment of the waterplane area divided by the displaced volume:

$$BM = \frac{I_T}{\nabla} = \frac{C_w \cdot L \cdot B^3 / 12}{\nabla}$$

This is exact for a wall-sided hull and a good approximation for ships of moderate form.

### Table-based hydrostatics (CSV loaded)

When a hydrostatic table is present (`ship->hydro->loaded == 1`), the code interpolates $KB$ and $BM$ directly from the table at the computed draft — these values already account for the ship's actual hull shape:

```c
/* from src/analysis.c — table-based path */
r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);

HydroEntry he;
hydro_interpolate(ship->hydro, r.draft, &he);

r.kb = he.kb;
r.bm = he.bm;
```

### KG — the centre of gravity

$KG$ is computed in the single-pass cargo accumulation loop by summing vertical weight moments, then dividing:

```c
/* from src/analysis.c — KG computation */
float vertical_moment = ship->lightship_weight * ship->lightship_kg;

for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *c = &ship->cargo[i];
    if (c->pos_x < 0) continue;           /* skip unplaced items */

    float cz = c->pos_z + c->dimensions[2] / 2.0f;   /* centroid height */
    vertical_moment += c->weight * cz;
}

r.kg = vertical_moment / displacement_kg;
```

Each item's vertical centroid is its `pos_z` (base of the item, from the hull origin) plus half its height. Unplaced items (`pos_x < 0`) are excluded — they are not yet aboard.

### Assembling GM

With $KB$, $BM$, and $KG$ known, the formula is a single line in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c):

```c
/* GM before free surface correction */
r.gm = r.kb + r.bm - r.kg;
```

---

## Free-surface correction

Liquid in a partially filled tank sloshes when the ship heels. This shifts the effective centre of gravity upward — a **virtual** rise in $KG$ — reducing $GM$. The correction is calculated from the tank geometry:

$$\text{FSM} = \rho \cdot \frac{l \cdot b^3}{12} \qquad \text{(per tank)}$$

$$\text{FSC} = \frac{\sum \text{FSM}}{\Delta} \qquad \text{(virtual KG rise, metres)}$$

where $l$ and $b$ are the tank's length and breadth, $\rho$ is the liquid density, and $\Delta$ is the ship's displacement in tonnes. Empty and 100%-full tanks contribute zero (no free surface).

In [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c):

```c
r.free_surface_correction = 0.0f;
if (ship->tanks && ship->tanks->count > 0) {
    r.free_surface_correction = calculate_virtual_kg_rise(ship->tanks, displacement_t);
}
r.gm_corrected = r.gm - r.free_surface_correction;
```

Every subsequent calculation — heel, GZ curve, IMO criteria — uses `gm_corrected`, not the raw `gm`. This is why the output report prints both:

```
  GM (transverse)       : 1.43 m
  Free surface corr     : -0.089 m
  GM (corrected)        : 1.34 m
```

!!! note "Always correct for free surface"
    Forgetting the FSC is a real-world loading error. Ballast tanks are typically kept either empty or full — a half-filled double-bottom tank can reduce GM by 0.1–0.3 m, which can be the margin between pass and fail on IMO criteria.

---

## The GZ curve — stability beyond small angles

$GM$ is only a linearisation valid for small heels (roughly below 10–15°). At larger angles, the righting lever $GZ(\theta)$ is computed via the **wall-sided formula**:

$$GZ(\theta) = \sin\theta \cdot \left[ GM + \frac{BM}{2} \tan^2\theta \right]$$

```c
/* from src/analysis.c */
static float gz_at_angle(float gm, float bm, float theta_deg) {
    float theta = theta_deg * (float)M_PI / 180.0f;
    float tan_theta = tanf(theta);
    return sinf(theta) * (gm + bm * tan_theta * tan_theta / 2.0f);
}
```

The $BM \tan^2\theta / 2$ term accounts for the increasing waterplane inertia effect as the ship heels — this is what makes the GZ curve rise above the small-angle linear approximation at moderate angles.

---

## IMO criteria: the regulatory floor

International regulations (IMO Resolution MSC.267/85, Part A, Chapter 2.2) define six minimum thresholds that every cargo vessel must meet. CargoForge-C checks all six using `gm_corrected`:

```c
/* from src/analysis.c */
r.imo_compliant = (
    gm_effective   >= IMO_GM_MIN &&        /* >= 0.15 m */
    r.gz_at_30     >= IMO_GZ_AT_30_MIN &&  /* >= 0.20 m */
    r.gz_max_angle >= IMO_GZ_MAX_ANGLE &&  /* >= 25 deg  */
    r.area_0_30    >= IMO_AREA_0_30_MIN && /* >= 0.055 m·rad */
    r.area_0_40    >= IMO_AREA_0_40_MIN && /* >= 0.090 m·rad */
    r.area_30_40   >= IMO_AREA_30_40_MIN   /* >= 0.030 m·rad */
) ? 1 : 0;
```

| Criterion | Threshold | Why it matters |
|-----------|-----------|----------------|
| $GM \geq 0.15$ m | `IMO_GM_MIN` | Minimum self-righting ability at small angles |
| $GZ_{30°} \geq 0.20$ m | `IMO_GZ_AT_30_MIN` | Adequate lever at a realistic wave-heel angle |
| Angle of max GZ $\geq 25°$ | `IMO_GZ_MAX_ANGLE` | Righting arm must peak late enough to resist large rolls |
| Area 0–30° $\geq 0.055$ m·rad | `IMO_AREA_0_30_MIN` | Energy absorbed in early heeling |
| Area 0–40° $\geq 0.090$ m·rad | `IMO_AREA_0_40_MIN` | Total energy reserve up to steep heel |
| Area 30–40° $\geq 0.030$ m·rad | `IMO_AREA_30_40_MIN` | Reserve between 30° and 40° — guards against "dead ship" scenarios |

The area criteria capture energy, not just force: a wider, flatter GZ curve survives a wave better than a tall, narrow one with the same peak.

---

## What "too tender" and "too stiff" mean

`print_loading_plan` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) classifies the corrected GM for the human-readable report:

```c
if (gm_display < 0.3f)       gm_str = "CRITICAL - Too tender";
else if (gm_display > 3.0f)  gm_str = "WARNING - Too stiff";
else if (gm_display >= 0.5f && gm_display <= 2.5f) gm_str = "Optimal";
else                          gm_str = "Acceptable";
```

- **Too tender** ($GM < 0.3$ m): slow, wallowing roll with little reserve. A heavy sea could capsize the ship.
- **Optimal** ($0.5 \leq GM \leq 2.5$ m): comfortable roll period, good reserve.
- **Too stiff** ($GM > 3.0$ m): short, violent snap-roll. Structurally hard on cargo, crew, and fastenings — and counterintuitively dangerous if a high sea coincides with the ship's roll period (resonance).

Stiffness is often a bigger practical concern for cargo operators than tenderness: heavy cargo concentrated low makes a stiff ship, which produces acceleration loads that damage cargo and crew.

---

## Recap

- $GM = KB + BM - KG$: height of the metacentre above the keel, minus height of the centre of gravity.
- $GM > 0$ means the ship self-rights; $GM \leq 0$ means it does not.
- `perform_analysis` computes $KB$ and $BM$ either analytically (box-hull constants) or by interpolating a hydrostatic CSV table, and $KG$ from a single-pass weight-moment sum over all placed cargo.
- The free-surface correction reduces $GM$ by a virtual KG rise from sloshing liquid; all IMO criteria use the corrected value `gm_corrected`.
- The GZ curve extends stability assessment beyond small angles using the wall-sided formula; `gz_at_angle` implements it in four lines of C.
- IMO MSC.267/85 sets six thresholds; `imo_compliant = 1` only when all six pass simultaneously.

*Next: [Trim and heel](24-trim-and-heel.md).*
