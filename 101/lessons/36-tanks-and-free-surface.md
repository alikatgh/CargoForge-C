# Tanks and free surface

Ships carry liquid in tanks — ballast water, fuel oil, fresh water, slop.
When a tank is only partially filled, the liquid inside can slosh from side to side
as the ship rolls, effectively raising the centre of gravity and reducing stability.
CargoForge-C models this effect through the **free-surface correction**, computed in
[`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) and folded into every stability analysis that includes a tank file.

---

## The physical problem

Imagine a rectangular tank half full of water.
When the ship heels to starboard, the water surface stays horizontal while the tank tilts
beneath it — liquid shifts to the low side.
This transfer of mass acts exactly as if the ship's own centre of gravity had risen by a
small amount, reducing the righting lever $GZ$ at every angle.

The shift is captured by the **free-surface moment (FSM)**, a property of the tank
geometry and its liquid density — crucially, *independent of fill level* (as long as the
tank is neither empty nor completely full).

---

## The rectangular-tank FSM formula

For a rectangular tank with length $l$, breadth $b$, and liquid density $\rho$:

$$
\text{FSM} = \rho \cdot \frac{l \cdot b^3}{12}
$$

The $b^3$ term is the second moment of area of the free surface about its own centreline.
A wide tank contributes far more FSM than a long, narrow one of the same volume — breadth
is cubed, length is only linear.

`calculate_free_surface_moment` in [`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) implements this directly:

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

Two things stand out:

1. **The fill fraction does not appear in the formula.** A tank that is 10% full produces
   the same FSM as one that is 90% full — only the geometry and density matter.
2. **Empty and full tanks are explicitly excluded.** At 0% fill there is no free surface
   at all; at 100% fill the liquid is rigid against the tank top and cannot shift.
   Both cases return 0.

!!! note
    The fill-fraction independence is counterintuitive but correct for a rectangular
    prismatic tank.  The second moment of area of a rectangle about its centre is
    $l b^3 / 12$ regardless of how deep the liquid sits — the *shape* of the surface
    is always $l \times b$.

---

## Why full and empty tanks have zero effect

A full tank behaves like solid ballast: the liquid has no room to move, so there is no
transfer of mass during a heel.
An empty tank has no liquid, so there is nothing to move.
Only the partially filled case creates a mobile free surface, which is why the guard
`fill_fraction <= 0.0f || fill_fraction >= 1.0f` returns 0 immediately.

In practice this means the ship operator can *eliminate* free-surface losses on any tank
by either pumping it completely full or completely emptying it — a real technique used
before heavy-weather passages.

---

## From FSM to virtual KG rise

The total FSM across all tanks is divided by the ship's displacement to obtain a **virtual
KG rise** in metres:

$$
\text{FSC} = \frac{\sum \text{FSM}_i}{\Delta}
$$

where $\Delta$ is displacement in tonnes.  The corrected metacentric height is then:

$$
GM_{\text{corrected}} = GM - \text{FSC}
$$

[`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) provides two helpers that carry this out:

```c
float calculate_total_fsm(const TankConfig *config) {
    float total = 0.0f;
    for (int i = 0; i < config->count; i++)
        total += calculate_free_surface_moment(&config->tanks[i]);
    return total;
}

float calculate_virtual_kg_rise(const TankConfig *config, float displacement_t) {
    if (!config || displacement_t < 0.01f) return 0.0f;
    float total_fsm = calculate_total_fsm(config);
    return total_fsm / displacement_t;
}
```

`perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) calls `calculate_virtual_kg_rise`, stores the
result in `AnalysisResult.free_surface_correction`, and subtracts it from GM to produce
`AnalysisResult.gm_corrected`.  Every downstream criterion — GZ curve, IMO checks,
heel angle — uses `gm_corrected`, never the uncorrected GM.

---

## The tank CSV format

Tanks are supplied as a comma-separated file referenced by the `tank_config` key in the
ship config.  Each row describes one tank; all nine fields are required:

```
id, length_m, breadth_m, height_m, pos_x_m, pos_y_m, pos_z_m, fill_fraction, density_t_per_m3
```

A minimal example with two tanks — a fuel-oil double-bottom and a seawater ballast wing:

```
# Tank CSV — units: metres, fill fraction 0-1, density t/m³
DB_FuelOil,   12.0, 8.0, 1.5,  45.0, 0.0, -9.5, 0.60, 0.85
PS_Ballast,    8.0, 3.5, 4.0,  20.0, 8.5, -8.0, 0.40, 1.025
```

`parse_tank_config` in [`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) reads this file line by line with `sscanf`:

```c
int parsed = sscanf(line, "%31[^,],%f,%f,%f,%f,%f,%f,%f,%f",
                    id_buf, &t->length, &t->breadth, &t->height,
                    &t->pos_x, &t->pos_y, &t->pos_z,
                    &t->fill_fraction, &t->density);

if (parsed < 9) {
    fprintf(stderr, "Warning: Skipping malformed tank entry at line %d\n", line_num);
    continue;
}
```

If `sscanf` returns fewer than 9 matches the row is skipped with a warning rather than
aborting the parse.  The fill fraction is immediately clamped to $[0, 1]$:

```c
if (t->fill_fraction < 0.0f) t->fill_fraction = 0.0f;
if (t->fill_fraction > 1.0f) t->fill_fraction = 1.0f;
```

Lines beginning with `#` or blank lines are ignored, so comments are supported.

---

## Tank weight and vertical moment

Tanks also contribute mass and a vertical moment to the KG calculation.
`calculate_tank_weight` sums the actual liquid volume times density across all tanks:

```c
float volume = t->length * t->breadth * t->height * t->fill_fraction;
total += volume * t->density;
```

`calculate_tank_vertical_moment` computes where that mass acts vertically.
The centroid of the liquid column in a partially filled rectangular tank sits at half the
fill height above the tank bottom:

```c
float liquid_cg = t->pos_z + (t->height * t->fill_fraction) / 2.0f;
moment += weight * liquid_cg;
```

`perform_analysis` adds these contributions directly into the global vertical moment
before computing KG, so heavy ballast low in the ship correctly lowers KG even as its
free surface adds a correction back through FSC.

---

## Density values in practice

| Liquid | Density (t/m³) |
|---|---|
| Seawater ballast | 1.025 |
| Fresh water | 1.000 |
| Heavy fuel oil (IFO 380) | 0.991 |
| Marine diesel / gas oil | 0.850 |
| Lube oil | 0.900 |

The CSV file carries the density per tank, so a ship with both fuel-oil and ballast tanks
models each correctly.  CargoForge-C does not impose any default — the operator must
supply the right value.

---

## Worked example

A ship with displacement $\Delta = 12\,000$ t carries two partially filled tanks:

| Tank | $l$ (m) | $b$ (m) | $\rho$ (t/m³) | FSM (t·m) |
|---|---|---|---|---|
| DB_FuelOil | 12.0 | 8.0 | 0.85 | $0.85 \times 12 \times 8^3 / 12 = 435.2$ |
| PS_Ballast | 8.0 | 3.5 | 1.025 | $1.025 \times 8 \times 3.5^3 / 12 = 29.3$ |

$$
\text{FSC} = \frac{435.2 + 29.3}{12\,000} = \frac{464.5}{12\,000} \approx 0.039 \text{ m}
$$

If the uncorrected GM was 1.20 m, the corrected value is $1.20 - 0.039 = 1.161$ m —
still comfortably above the IMO minimum of 0.15 m, but the fuel-oil double-bottom alone
costs almost 40 mm of metacentric height.

!!! warning
    A single wide slack tank can consume several tenths of a metre of GM.  On a vessel
    already close to the IMO limits, failing to account for free surface can produce an
    `imo_compliant = 0` result that disappears entirely when the tanks are topped off or
    emptied.  Always verify the tank fill fractions in the CSV reflect the actual loading
    condition.

---

## Recap

- The **free-surface moment** $\rho \cdot l \cdot b^3 / 12$ quantifies how much a
  partially filled rectangular tank degrades stability; breadth dominates because it is
  cubed.
- **Empty and completely full tanks contribute zero FSM** — there is no mobile free
  surface to create the shift.
- The **virtual KG rise** (FSC = total FSM / displacement) is subtracted from GM to give
  $GM_{\text{corrected}}$, which is what CargoForge-C uses for every stability criterion.
- The **tank CSV** requires nine fields per row: id, length, breadth, height, three
  position coordinates, fill fraction, and density; fill fraction is clamped to $[0, 1]$.
- Tank mass and its vertical moment are included in the global KG calculation, so ballast
  low in the ship still lowers the centre of gravity even as its free surface partially
  offsets the benefit.

*Next: [Longitudinal strength](37-longitudinal-strength.md).*
