# Free surface and the IMO criteria

A ship's tanks are rarely empty or brim-full. When a tank is partially filled, the liquid inside sloshes — and that sloshing effectively raises the vessel's centre of gravity even though no mass has moved upward. CargoForge-C models this effect through the **free-surface correction** computed in [`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c), then tests the corrected stability against six mandatory IMO thresholds checked in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c).

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:580px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Free surface effect: sloshing liquid raises the effective centre of gravity</title>
<desc>A partly filled tank. Upright, the liquid's centroid sits on the centreline. When the ship heels, the liquid surface stays horizontal so the liquid forms a wedge toward the low side and its centroid shifts there. That shift acts like raising the ship's centre of gravity, which reduces GM. CargoForge-C subtracts a virtual rise computed from the free-surface moment.</desc>
<defs><marker id="fs-ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#D05663"/></marker></defs>
<text x="40" y="30" fill="currentColor" font-size="12" font-weight="600">upright</text>
<text x="330" y="30" fill="currentColor" font-size="12" font-weight="600">heeled</text>
<!-- upright tank -->
<rect x="48" y="48" width="180" height="110" rx="3" fill="none" stroke="currentColor" stroke-width="1.3" opacity="0.8"/>
<rect x="49" y="106" width="178" height="51" fill="#12A594" fill-opacity="0.18"/>
<line x1="49" y1="106" x2="227" y2="106" stroke="#12A594" stroke-width="1.3"/>
<circle cx="138" cy="130" r="4" fill="#D05663"/><text x="138" y="148" fill="#D05663" font-size="10" text-anchor="middle">g</text>
<!-- heeled tank -->
<g transform="rotate(13 420 110)">
<rect x="330" y="48" width="180" height="110" rx="3" fill="none" stroke="currentColor" stroke-width="1.3" opacity="0.8"/>
</g>
<!-- liquid surface stays horizontal -> wedge; draw shifted liquid + g -->
<path d="M338,150 L500,113 L500,158 L338,170 Z" fill="#12A594" fill-opacity="0.18"/>
<line x1="338" y1="150" x2="500" y2="113" stroke="#12A594" stroke-width="1.3"/>
<circle cx="452" cy="142" r="4" fill="#D05663"/><text x="452" y="160" fill="#D05663" font-size="10" text-anchor="middle">g</text>
<line x1="392" y1="132" x2="446" y2="142" stroke="#D05663" stroke-width="1.4" stroke-dasharray="3 2" marker-end="url(#fs-ar)"/>
<text x="540" y="100" fill="currentColor" font-size="10.5" opacity="0.7" text-anchor="end">g shifts low</text>
<text x="300" y="196" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.7">The shift acts like raising KG, so <tspan font-family="var(--md-code-font,monospace)">GM</tspan> drops:</text>
<text x="300" y="213" fill="#12A594" font-size="11.5" text-anchor="middle" font-weight="600">GM_corrected = GM − FSC,  FSM = ρ·l·b³⁄12  (src/tanks.c)</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Free-surface effect** = "sloshing liquid that acts as if it lifted the whole ship's centre of gravity" — when a partially-filled tank heels, its liquid shifts toward the low side; `perform_analysis` in `src/analysis.c` treats that shift as a virtual KG rise, reducing the righting ability even though no mass actually moved upward.
- **Free-surface moment (FSM)** = "a number that measures how destabilising a half-full tank is" — `calculate_free_surface_moment` in `src/tanks.c` computes it as `density × length × breadth³ / 12`; breadth appears cubed, so a wide tank is far more dangerous than a long, narrow one of the same volume.
- **Free-surface correction (FSC)** = "the total virtual KG rise summed across all partially-filled tanks" — `calculate_virtual_kg_rise` divides the sum of all FSMs by the ship's displacement; `perform_analysis` subtracts this from raw GM to get `gm_corrected`, the only GM value used in every subsequent check.
- **GM corrected** = "how much the ship will resist rolling once tank sloshing is accounted for" — if `gm_corrected` drops below 0.15 m the first IMO criterion fails and `imo_compliant` is set to 0, meaning the loading plan must be revised before departure.
- **GZ (righting lever)** = "the sideways push that brings a heeled ship back upright" — `gz_at_angle` computes it from `gm_corrected` using the wall-sided formula; the six IMO thresholds test not just its size at one angle but the area under its curve, ensuring the ship can absorb wave energy across a range of heels.
- **The six IMO criteria** = "six simultaneous pass/fail tests that together prove the ship won't capsize in realistic seas" — `perform_analysis` evaluates all six (GM floor, GZ at 30°, angle of maximum GZ, and three righting-energy areas) and only sets `imo_compliant = 1` when every one passes; a single failure is enough to reject the plan.
- **Deck weight ratio and stack pressure constraints** = "cargo placement rules in `src/constraints.c` that keep KG low before the stability solver even runs" — `check_cargo_constraints` enforces a 40% deck-weight cap and a 10 t/m² stack-pressure limit during `find_best_fit_3d`, so problematic cargo is rejected at placement time, not discovered after the fact.

**Why it matters:** if you skip the free-surface correction you get a falsely optimistic GM — in a vessel with several large double-bottom tanks partially filled the FSC can exceed 0.5 m, turning a compliant loading plan into a capsize risk without any visible change in cargo weight.

---

## Why partial fill is dangerous

Imagine a rectangular tank half-full of fuel. If the ship heels slightly to starboard, liquid flows toward the low side. The ship's total weight has not changed, but the centre of that liquid shifted transversely. The result is a reduction in the **righting lever** at every angle of heel — as if the entire ship's centre of gravity had risen by some amount $\delta KG$. This is the **free-surface effect**.

The key insight: the magnitude of the effect depends on the **plan-view moment of inertia** of the liquid surface — essentially how wide the tank is. A long, narrow tank causes little trouble; a wide, shallow one can be deadly.

---

## The physics: free-surface moment (FSM)

For a rectangular tank the transverse second moment of area of the free surface is:

$$I_T = \frac{l \cdot b^3}{12}$$

where $l$ is the tank length (fore-aft) and $b$ is the tank breadth (transverse). Multiplying by the liquid density $\rho$ gives the **free-surface moment**:

$$FSM = \rho \cdot \frac{l \cdot b^3}{12}$$

Two things to notice:

- Breadth appears **cubed**. Doubling a tank's width multiplies its FSM by eight. Width is the dominant variable.
- Fill level does **not** appear. For a rectangular tank, the free-surface moment is the same whether the tank is 10% or 90% full — only the presence of a free surface matters, not its depth.

The exceptions are the boundary cases: an empty tank has no free surface, and a 100%-full tank has no free surface either. Both contribute zero FSM.

### How CargoForge-C computes it

[`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) implements this directly in `calculate_free_surface_moment`:

```c
float calculate_free_surface_moment(const Tank *tank) {
    if (!tank) return 0.0f;

    /* No free surface for empty or full tanks */
    if (tank->fill_fraction <= 0.0f || tank->fill_fraction >= 1.0f)
        return 0.0f;

    /* FSM = rho * l * b^3 / 12 for rectangular tank */
    return tank->density * tank->length
           * tank->breadth * tank->breadth * tank->breadth / 12.0f;
}
```

The guard on `fill_fraction` enforces the boundary-case rule. `tank->density` is the liquid density in t/m³ (typically `1.025` for seawater ballast, `0.85` for fuel oil — both parsed from the tank CSV).

`calculate_total_fsm` sums the FSM over all tanks:

```c
float calculate_total_fsm(const TankConfig *config) {
    if (!config) return 0.0f;

    float total = 0.0f;
    for (int i = 0; i < config->count; i++) {
        total += calculate_free_surface_moment(&config->tanks[i]);
    }
    return total;
}
```

---

## Converting FSM to a KG rise

The free-surface correction (FSC) is the virtual rise in the ship's centre of gravity caused by all partially-filled tanks combined:

$$FSC = \frac{\sum FSM}{\Delta}$$

where $\Delta$ is the ship's displacement in tonnes. The corrected metacentric height is then:

$$GM_{corrected} = GM - FSC = (KB + BM - KG) - FSC$$

[`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) provides `calculate_virtual_kg_rise` to produce this number:

```c
float calculate_virtual_kg_rise(const TankConfig *config, float displacement_t) {
    if (!config || displacement_t < 0.01f) return 0.0f;

    float total_fsm = calculate_total_fsm(config);
    return total_fsm / displacement_t;
}
```

`perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) stores the result as `result.free_surface_correction` and subtracts it to obtain `result.gm_corrected`. Every subsequent stability criterion — GZ curve, IMO checks, heel angle — uses `gm_corrected`, never the raw GM.

!!! warning "Never use uncorrected GM for seakeeping decisions"
    Raw $GM = KB + BM - KG$ ignores tank sloshing entirely. In a vessel with several large double-bottom tanks partially filled, the FSC can exceed 0.5 m. Using the uncorrected figure would give a falsely optimistic stability picture.

---

## Tank weight and vertical moment

Partially-filled tanks also contribute real mass to the ship's displacement and shift its vertical centre of gravity. [`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) computes both.

`calculate_tank_weight` finds the total liquid mass in tonnes:

```c
float calculate_tank_weight(const TankConfig *config) {
    if (!config) return 0.0f;

    float total = 0.0f;
    for (int i = 0; i < config->count; i++) {
        const Tank *t = &config->tanks[i];
        float volume = t->length * t->breadth * t->height * t->fill_fraction;
        total += volume * t->density;
    }
    return total;
}
```

`calculate_tank_vertical_moment` locates the liquid's centre of gravity. For a rectangular tank the liquid column fills from the tank floor (`pos_z`) upward to a height of `t->height * t->fill_fraction`, so its centroid sits halfway up that column:

```c
float calculate_tank_vertical_moment(const TankConfig *config) {
    /* ... */
    float liquid_cg = t->pos_z + (t->height * t->fill_fraction) / 2.0f;
    moment += weight * liquid_cg;
    /* ... */
}
```

`perform_analysis` adds both into the displacement and KG calculation before computing GM, so the physical weight of the liquid and the virtual KG rise from its free surface are handled as separate, additive effects.

---

## The six IMO intact-stability criteria

Once CargoForge-C has $GM_{corrected}$, it evaluates the ship's GZ (righting lever) curve using the wall-sided formula:

$$GZ(\theta) = \sin\theta \cdot \left(GM_{corrected} + \frac{BM \cdot \tan^2\theta}{2}\right)$$

Six thresholds from IMO Resolution MSC.267(85), Part A, Chapter 2.2 must all pass. `perform_analysis` stores each computed value in an `AnalysisResult` and sets `imo_compliant = 1` only when all six are satisfied:

| Criterion | Threshold | `AnalysisResult` field |
|-----------|-----------|------------------------|
| Corrected GM | $\geq 0.15$ m | `gm_corrected` |
| GZ at 30° heel | $\geq 0.20$ m | `gz_at_30` |
| Angle of maximum GZ | $\geq 25°$ | `gz_max_angle` |
| Area under GZ curve, 0–30° | $\geq 0.055$ m·rad | `area_0_30` |
| Area under GZ curve, 0–40° | $\geq 0.090$ m·rad | `area_0_40` |
| Area under GZ curve, 30–40° | $\geq 0.030$ m·rad | `area_30_40` |

The areas are computed by trapezoidal integration over 100 steps per interval. Negative GZ contributions (the ship has lost initial stability at that angle) are clamped to zero before summing — they represent a capsized state beyond analysis.

!!! note "Why six criteria instead of one?"
    A single GM check could be satisfied by a vessel that is initially stiff but loses righting ability rapidly past 20°. The area criteria force adequate energy absorption across a range of angles, protecting against both initial capsize (the 0–30° area) and progressive flooding past the point of no return (the 30–40° area).

---

## Constraints that indirectly protect stability

Placement decisions in [`src/constraints.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/constraints.c) also guard stability, even though they operate at the per-cargo level rather than the whole-ship level.

**Deck weight ratio.** The Deck bin has a weight limit of 40% of the ship's `max_weight`. `check_cargo_constraints` enforces this:

```c
if (strcmp(bin->name, "Deck") == 0) {
    float deck_weight_ratio = (bin->current_weight + cargo->weight) / ship->max_weight;
    if (deck_weight_ratio > MAX_DECK_WEIGHT_RATIO) {
        fprintf(stderr, "Constraint: Deck weight exceeds %.0f%% limit\n",
                MAX_DECK_WEIGHT_RATIO * 100.0f);
        return 0;
    }
}
```

Loading too much weight high on deck raises KG and shrinks GM. Rejecting excess deck cargo keeps KG low before the stability analysis even runs.

**Stacking pressure.** `calculate_stack_pressure` accumulates the weight of all already-placed cargo above a candidate position, prorated by the overlapping footprint fraction, and rejects the placement if the resulting pressure exceeds `MAX_STACK_PRESSURE` (10 t/m²) — or a tighter limit for fragile cargo. Beyond protecting structural integrity, this prevents pathologically tall stacks that would push cargo's centre of gravity far above the waterline.

**Point load.** A piece of cargo whose weight per unit footprint exceeds `MAX_POINT_LOAD` (5 t/m²) is rejected outright:

```c
float point_load = calculate_point_load(cargo);
if (point_load > MAX_POINT_LOAD) { return 0; }
```

`calculate_point_load` divides `cargo->weight` (kg, converted to tonnes) by the footprint area (`dimensions[0] × dimensions[1]`). This prevents concentrated loads from punching through hatch covers and deck plating — a structural failure that would immediately compromise watertight integrity.

These constraints are checked during bin-packing in `find_best_fit_3d`, so cargo that would harm stability is rejected before a position is committed, not flagged after the fact.

---

## End-to-end flow

```
parse_tank_config()
       |
       v
calculate_tank_weight()          ──► added to displacement_kg in perform_analysis
calculate_tank_vertical_moment() ──► added to KG moment accumulator
calculate_virtual_kg_rise()      ──► stored as free_surface_correction
       |
       v
GM = KB + BM - KG
GM_corrected = GM - free_surface_correction
       |
       v
gz_at_angle()  ─► GZ curve  ─► integrate_gz() / find_gz_max()
       |
       v
IMO six-criterion check  ─► imo_compliant = 0 or 1
```

If no tank CSV is loaded (`ship->tanks == NULL`), `calculate_virtual_kg_rise` returns 0 and the correction is skipped — the tool assumes a rigid ship with no free surfaces.

---

## Recap

- The **free-surface moment** $FSM = \rho \cdot l \cdot b^3 / 12$ is independent of fill level; only an empty or completely full tank contributes zero.
- Breadth dominates: doubling a tank's width multiplies its FSM eightfold.
- The **free-surface correction** $FSC = \sum FSM / \Delta$ is subtracted from GM to give $GM_{corrected}$, which is used for every subsequent criterion.
- CargoForge-C checks **six IMO MSC.267(85) criteria** — GM floor, GZ at 30°, angle of max GZ, and three righting-energy areas — setting `imo_compliant` only when all six pass.
- Placement constraints in `constraints.c` (deck weight ratio, stacking pressure, point load) act as a first line of defence, keeping KG low before the stability solver runs.
- If no tank file is loaded, the free-surface correction is zero; the field is silently skipped, not an error.

*Next: [Lab 6 — Compute GM, then break it](lab-06-stability.md).*
