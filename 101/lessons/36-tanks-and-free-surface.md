# Tanks and free surface

Ships carry liquid in tanks — ballast water, fuel oil, fresh water, slop.
When a tank is only partially filled, the liquid inside can slosh from side to side
as the ship rolls, effectively raising the centre of gravity and reducing stability.
CargoForge-C models this effect through the **free-surface correction**, computed in
[`src/tanks.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/tanks.c) and folded into every stability analysis that includes a tank file.

## The mental model 🧠

A slack tank is a small loose sea riding inside the ship. Fill it to the brim or drain it dry and the liquid is locked in place — honest cargo. Leave it part-full and the liquid has a free surface that runs downhill every time the ship rolls, shoving weight to the low side and acting exactly as if the centre of gravity had been lifted. CargoForge charges that as a **virtual rise in KG**, subtracted from GM before any check runs.

The surprise is what the penalty depends on. The free-surface moment is `ρ · l · b³ / 12` — the breadth is *cubed*, so a tank's *width* matters enormously and its *fill level* barely at all: a wide shallow puddle is far more dangerous than a deep narrow one of the same volume. That cube is also the cure — split a wide tank with one lengthwise baffle into two half-width tanks and the penalty drops by roughly *four* (because (b/2)³ × 2 = b³/4). It is why real ships are full of baffles.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Free surface in a slack tank, and how a baffle cuts the penalty</title>
<desc>A partly filled tank lets liquid slosh to the low side, which acts like raising the ship's centre of gravity. The free-surface moment grows with the cube of the surface breadth, so a single lengthwise baffle that halves the breadth cuts the penalty to about a quarter.</desc>
<text x="145" y="28" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.75">slack tank — wide surface</text>
<rect x="40" y="44" width="210" height="100" rx="3" fill="none" stroke="currentColor" stroke-opacity="0.6" stroke-width="1.2"/>
<path d="M42,142 L248,142 L248,104 L42,90 Z" fill="#12A594" fill-opacity="0.18" stroke="#12A594" stroke-opacity="0.5"/>
<line x1="42" y1="160" x2="248" y2="160" stroke="currentColor" stroke-opacity="0.5"/><path d="M49,156 L41,160 L49,164" fill="none" stroke="currentColor" stroke-opacity="0.5"/><path d="M241,156 L249,160 L241,164" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<text x="145" y="174" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.7">b  (full breadth)</text>
<line x1="266" y1="138" x2="266" y2="86" stroke="#D05663" stroke-width="1.6"/><path d="M261,93 L266,82 L271,93" fill="#D05663"/>
<text x="276" y="106" font-size="9" fill="#D05663">G rise</text>
<text x="276" y="118" font-size="9" fill="#D05663">(large)</text>
<text x="445" y="28" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.75">same tank, one baffle</text>
<rect x="338" y="44" width="210" height="100" rx="3" fill="none" stroke="currentColor" stroke-opacity="0.6" stroke-width="1.2"/>
<line x1="443" y1="44" x2="443" y2="144" stroke="currentColor" stroke-opacity="0.6" stroke-width="1.2"/>
<path d="M340,142 L441,142 L441,116 L340,110 Z" fill="#12A594" fill-opacity="0.18" stroke="#12A594" stroke-opacity="0.5"/>
<path d="M445,142 L546,142 L546,116 L445,110 Z" fill="#12A594" fill-opacity="0.18" stroke="#12A594" stroke-opacity="0.5"/>
<text x="392" y="174" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.7">b/2</text>
<text x="496" y="174" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.7">b/2</text>
<line x1="560" y1="130" x2="560" y2="104" stroke="#12A594" stroke-width="1.6"/><path d="M555,111 L560,100 L565,111" fill="#12A594"/>
<text x="556" y="124" font-size="9" text-anchor="end" fill="#12A594">G rise (¼)</text>
<text x="300" y="202" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.75" font-family="var(--md-code-font,monospace)">FSM = ρ · l · b³ / 12   —   breadth cubed: halve b → penalty ~¼</text>
</svg>

<div class="lesson-widget" data-widget="free-surface-explorer"></div>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Free surface** = "liquid that can slosh around inside a partly filled tank" — when the ship tilts, that sloshing shifts weight to the low side and makes the ship even less stable, as if its centre of gravity had been lifted a little higher.
- **Free-surface moment (FSM)** = "a number that measures how destabilising a slack tank is" — `calculate_free_surface_moment` computes it as $\rho \cdot l \cdot b^3 / 12$; the breadth is cubed, so a wide tank is far more dangerous than a long narrow one of the same volume.
- **Fill-fraction independence** = "a tank that is 10% full is just as destabilising as one that is 90% full" — the FSM formula contains the tank's geometry, not its fill level; only the extreme cases (0% or 100% full) return zero, because then the liquid has no room to move.
- **Virtual KG rise / free-surface correction (FSC)** = "the effective penalty in metres that slack tanks add to the ship's centre of gravity" — `calculate_virtual_kg_rise` sums all FSMs and divides by the ship's displacement, giving the value that `perform_analysis` subtracts from GM to produce `gm_corrected`.
- **GM corrected** = "the stability margin the ship actually has, after accounting for all sloshing tanks" — every downstream check in CargoForge-C (IMO criteria, GZ curve, heel angle) uses `gm_corrected`, never the raw GM.
- **Tank CSV** = "the nine-column input file that tells CargoForge-C about each tank's size, position, fill level, and liquid type" — `parse_tank_config` reads it and clamps fill fraction to $[0, 1]$, so an out-of-range value is silently corrected rather than crashing the analysis.

**Why it matters:** A single wide, slack tank can steal several tenths of a metre of GM; on a heavily loaded vessel that can flip an IMO-compliant result to non-compliant. Getting the fill fractions wrong in the CSV — or forgetting to model tanks at all — produces an optimistic stability picture that does not match the real ship.

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

## Check yourself

??? question "Why does breadth matter so much more than fill level in the free-surface formula?"
    The formula is ρ·l·b³/12 — breadth is *cubed*, so a wide shallow tank is far more dangerous than a deep narrow one of the same volume. Fill level doesn't even appear as a variable: the penalty is constant for any partial fill and drops to zero only right at the 0%/100% extremes.

??? question "Why does one lengthwise baffle cut the free-surface penalty to roughly 1/4, not 1/2?"
    Each of the two resulting half-width compartments has (b/2)³ = b³/8 of the original moment, but there are now two of them: 2 × b³/8 = b³/4. The cube shrinks each half faster than the doubling adds back, so the total lands at a quarter, not a half.

*Next: [Longitudinal strength](37-longitudinal-strength.md).*
