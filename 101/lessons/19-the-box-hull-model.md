# The box-hull model

When CargoForge-C does not have a ship's measured hydrostatic table, it still needs to answer questions like "how deep will the ship sit in the water?" and "how stable is it?". It answers them by pretending the hull is a rectangular box — a deliberately simplified shape whose geometry can be solved with a few multiplications. This lesson explains how that model works, where each formula comes from, and why the program uses it only as a fallback when real table data is absent.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Box-hull model** = "treat the ship as a rectangular swimming pool to do the maths" — when `ship->hydro` is `NULL` or not loaded, `perform_analysis` in `src/analysis.c` falls back to this shortcut so it can still compute draft, KB, BM, and GM without any measured data.
- **Block coefficient (`BLOCK_COEFF = 0.75`)** = "a real hull is only 75% as full as its bounding box" — the underwater body tapers at the bow and stern, so the code divides displaced volume by `L × B × 0.75` to get a draft that is correctly deeper than a perfect rectangle would give.
- **KB (centre of buoyancy height)** = "the average depth of the water the ship is pushing aside" — `perform_analysis` estimates it as `KB_FACTOR * draft` (0.53 × T), meaning the buoyant centroid sits just over halfway down the draft, which holds well for full-form cargo ships.
- **BM (metacentric radius)** = "how hard the waterplane shape fights a heel" — the code computes it as `inertia_t / displaced_vol`, where `inertia_t = (L × B³ / 12) × WATERPLANE_COEFF`; because breadth is cubed, doubling the beam roughly quadruples this resistance.
- **GM = KB + BM − KG** = "the total stability margin from keel to metacentre, minus how high the cargo sits" — `perform_analysis` assembles this from the box-hull KB and BM alongside the actual KG from placed cargo positions, then applies a free-surface correction before any IMO checks run.
- **`hydro_table_used` flag** = "which path did the code actually take?" — the program prints either "Hydrostatics: Table interpolation" or "Hydrostatics: Box-hull approximation" so operators always know whether the numbers came from measured data or a fleet-average rectangle.

**Why it matters:** if you supply a ship with an unusual hull form (a bulk carrier, a fast ferry) and no hydrostatic table, the box-hull constants may be off by 10–20%, producing a GM that looks safe when the real vessel is marginal — or vice versa; for any regulatory submission, always load a real table.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> Imagine sliding a brick into a bathtub of water. The brick sinks until it displaces its own weight in water. Now imagine shaving the brick's corners to mimic a ship's tapered bow and stern — the block coefficient `BLOCK_COEFF = 0.75` is exactly that "how much corner did we shave off" number. KB is just how far down the brick's centre sits underwater, and BM is how hard the brick's flat bottom fights tipping over.

In CargoForge-C the brick's dimensions are `ship->length` and `ship->width`; the shaved-corner factor is `BLOCK_COEFF`; and `KB_FACTOR * draft` places the buoyant centroid at 53% of the depth, not the exact middle, because real hulls are fuller at the bottom. BM is dominated by breadth cubed — the wider the brick, the more it resists rolling. Subtract KG (where the cargo sits, assembled from `parse_cargo_list`) and you have GM: the total distance from keel to metacentre, your single number for "is this ship stable?"

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Box-hull model cross-section</title>
  <desc>Schematic cross-section of the box-hull approximation showing draft T, key heights KB, KG, BM, and GM on the centreline, plus the waterplane breadth B.</desc>

  <!-- water fill -->
  <rect x="60" y="140" width="500" height="120" fill="#12A594" opacity="0.10"/>

  <!-- hull box (waterline at y=140, keel at y=260) -->
  <rect x="130" y="140" width="360" height="120" fill="none" stroke="currentColor" stroke-width="2"/>

  <!-- waterline -->
  <line x1="60" y1="140" x2="560" y2="140" stroke="currentColor" stroke-width="1" stroke-dasharray="6 4" opacity="0.5"/>
  <text x="565" y="144" font-size="11" fill="currentColor" opacity="0.7" dominant-baseline="middle">WL</text>

  <!-- keel line label -->
  <text x="65" y="264" font-size="11" fill="currentColor" opacity="0.7">K (keel)</text>

  <!-- centreline -->
  <line x1="310" y1="50" x2="310" y2="270" stroke="currentColor" stroke-width="1" stroke-dasharray="4 3" opacity="0.35"/>

  <!-- draft arrow (left side) -->
  <line x1="108" y1="140" x2="108" y2="260" stroke="currentColor" stroke-width="1.5" marker-end="url(#arr)" marker-start="url(#arr)"/>
  <text x="70" y="202" font-size="12" fill="currentColor" font-weight="600" dominant-baseline="middle">T (draft)</text>

  <!-- KB point -->
  <circle cx="310" cy="197" r="5" fill="#12A594"/>
  <line x1="310" y1="197" x2="395" y2="197" stroke="#12A594" stroke-width="1" stroke-dasharray="4 3"/>
  <text x="398" y="201" font-size="12" fill="#12A594" dominant-baseline="middle">KB = 0.53 × T</text>

  <!-- KG point (above waterline, representing cargo stacked high) -->
  <circle cx="310" cy="100" r="5" fill="#D05663"/>
  <line x1="310" y1="100" x2="395" y2="100" stroke="#D05663" stroke-width="1" stroke-dasharray="4 3"/>
  <text x="398" y="104" font-size="12" fill="#D05663" dominant-baseline="middle">KG (cargo centroid)</text>

  <!-- M (metacentre) point -->
  <circle cx="310" cy="58" r="5" fill="currentColor"/>
  <line x1="310" y1="58" x2="230" y2="58" stroke="currentColor" stroke-width="1" stroke-dasharray="4 3"/>
  <text x="70" y="62" font-size="12" fill="currentColor" dominant-baseline="middle">M (metacentre)</text>

  <!-- GM brace -->
  <line x1="340" y1="58" x2="340" y2="100" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <line x1="335" y1="58" x2="345" y2="58" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <line x1="335" y1="100" x2="345" y2="100" stroke="currentColor" stroke-width="1.5" opacity="0.6"/>
  <text x="347" y="82" font-size="12" fill="currentColor" font-weight="700" dominant-baseline="middle">GM</text>

  <!-- BM brace -->
  <line x1="272" y1="58" x2="272" y2="197" stroke="currentColor" stroke-width="1" opacity="0.45"/>
  <line x1="267" y1="58" x2="277" y2="58" stroke="currentColor" stroke-width="1" opacity="0.45"/>
  <line x1="267" y1="197" x2="277" y2="197" stroke="currentColor" stroke-width="1" opacity="0.45"/>
  <text x="232" y="130" font-size="11" fill="currentColor" opacity="0.75" dominant-baseline="middle">BM = I_T / ∇</text>

  <!-- breadth label -->
  <line x1="130" y1="278" x2="490" y2="278" stroke="currentColor" stroke-width="1.5" marker-end="url(#arr)" marker-start="url(#arr)"/>
  <text x="310" y="295" font-size="12" fill="currentColor" text-anchor="middle">B (breadth) — drives BM via B³</text>

  <!-- box label -->
  <text x="310" y="22" font-size="13" fill="currentColor" font-weight="700" text-anchor="middle">Box-hull cross-section (no hydrostatic table loaded)</text>

  <defs>
    <marker id="arr" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto">
      <path d="M0,0 L6,3 L0,6 Z" fill="currentColor"/>
    </marker>
  </defs>
</svg>

## Why approximate at all?

A real ship hull is a curved, tapered form. Naval architects measure its behavior at dozens of loading conditions and tabulate the results in a *hydrostatic table*: one row per draft, giving displacement, KB, BM, and so on. Loading CargoForge-C with such a table (via the `hydrostatic_table` key in the ship config) gives the most accurate answers.

But not every operator has that table, especially at early planning stages. The box-hull model lets CargoForge-C produce *useful, order-of-magnitude-correct* results from nothing but the ship's length, breadth, and a handful of correction coefficients. The code labels this path the "legacy box-hull fallback" — it has always existed, the table path was added later.

---

## The four constants

Everything in the box-hull model is controlled by four `#define` constants at the top of [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c):

```c
#define SEAWATER_DENSITY    1.025f  /* t/m3 at 15C */
#define BLOCK_COEFF         0.75f   /* Cb: underwater hull vol / bounding box */
#define WATERPLANE_COEFF    0.85f   /* Cw: waterplane area / L*B */
#define KB_FACTOR           0.53f   /* KB ~ 0.53*T for typical cargo ships */
```

Each constant is a ratio that corrects for the fact that a real hull is *not* a perfect box.

| Constant | Symbol | What it corrects |
|---|---|---|
| `BLOCK_COEFF` | $C_B$ | The underwater body is rounded; a cargo ship fills roughly 75% of its bounding box volume |
| `WATERPLANE_COEFF` | $C_W$ | The waterplane (the slice at the waterline) is also narrowed at bow and stern; roughly 85% of $L \times B$ |
| `KB_FACTOR` | — | The centre of buoyancy sits at about 53% of the draft for a typical cargo ship |
| `SEAWATER_DENSITY` | $\rho$ | Standard seawater at 15 °C; used to convert volume to tonnes |

These are fleet-average constants, not measurements of any specific vessel. They are good enough for the purpose of a planning tool.

---

## Step 1 — How deep will the ship sit? (Draft)

Before we can find draft we need displacement. `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) accumulates it from three sources — lightship weight, placed cargo, and ballast tanks — then computes draft via displaced volume:

```c
float displacement_t = displacement_kg / 1000.0f;
float displaced_vol  = displacement_t / SEAWATER_DENSITY;
```

By Archimedes' principle the ship displaces a volume of water whose mass equals the ship's total mass. Dividing mass (in tonnes) by water density (in t/m³) gives that volume in cubic metres.

Now we use the block coefficient to find the waterplane footprint the ship actually occupies:

```c
r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
```

In formula form:

$$T = \frac{\nabla}{L \times B \times C_B}$$

where $\nabla$ is displaced volume in m³, $L$ is ship length, $B$ is breadth, and $C_B = 0.75$. Because $C_B < 1$, the denominator is smaller than the raw $L \times B$, which means the computed draft is *deeper* than a perfect-box ship of the same size — matching the real world where hull volume tapers away from a rectangular cross-section.

---

## Step 2 — Where is the centre of buoyancy? (KB)

The centre of buoyancy (B) is the centroid of the submerged volume. For a rectangular cross-section filled uniformly to draft $T$, B sits at exactly $T/2$. Real ships are fuller at the bottom and taper toward the waterline, pushing B down slightly — hence the empirical factor:

```c
r.kb = KB_FACTOR * r.draft;
```

$$KB = 0.53 \times T$$

For a 200 m ship loaded to 8 m draft, $KB \approx 4.24$ m. This constant holds well for typical full-form cargo ships (block coefficient 0.70–0.80); it would overestimate KB for a shallow, wide car-carrier and underestimate it for a deep, narrow container ship.

---

## Step 3 — How wide is the waterplane? (BM)

BM is the transverse metacentric radius — the distance between the centre of buoyancy and the metacentre M. It tells you how strongly the hull geometry resists heeling. The formula is:

$$BM = \frac{I_T}{\nabla}$$

where $I_T$ is the second moment of area of the *waterplane* about the ship's centreline. For a rectangle of length $L$ and breadth $B$:

$$I_T = \frac{L \times B^3}{12}$$

But the waterplane is not a full rectangle, so we scale by $C_W$:

```c
float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
r.bm = inertia_t / displaced_vol;
```

The breadth appears *cubed* — making BM extremely sensitive to beam. A ship twice as wide has eight times the waterplane inertia; this is why wide, shallow vessels are inherently stiff.

The same pattern applies longitudinally for the trim calculation:

```c
float inertia_l = (ship->width * powf(ship->length, 3) / 12.0f) * WATERPLANE_COEFF;
bm_l = inertia_l / displaced_vol;
```

Here length is cubed and breadth is the linear dimension, because trim is resistance to rotation about a *transverse* axis.

---

## Step 4 — Putting it together (GM)

Once KB and BM are known, the transverse metacentric height is:

$$GM = KB + BM - KG$$

```c
r.gm = r.kb + r.bm - r.kg;
```

KG (the vertical centre of gravity) is computed earlier in the same function from the actual cargo positions — that part does not change between the box-hull and table-based paths. What changes is where KB and BM come from.

!!! note
    A free-surface correction is applied after this line. See the digest section on tanks for details. All stability criteria use `gm_corrected`, not `gm`.

---

## The branch in the code

The entire box-hull path sits in a single `else` clause that is reached only when `ship->hydro` is `NULL` or its `loaded` flag is `0`:

```c
if (ship->hydro && ship->hydro->loaded) {
    /* ---- Table-based hydrostatics ---- */
    r.hydro_table_used = 1;
    r.draft = hydro_draft_from_displacement(ship->hydro, displacement_t);
    HydroEntry he;
    hydro_interpolate(ship->hydro, r.draft, &he);
    r.kb = he.kb;
    r.bm = he.bm;
    /* ... MTC-based BM_L ... */
} else {
    /* ---- Legacy box-hull fallback ---- */
    r.hydro_table_used = 0;
    r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
    r.kb = KB_FACTOR * r.draft;
    float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
    r.bm = inertia_t / displaced_vol;
    float inertia_l = (ship->width * powf(ship->length, 3) / 12.0f) * WATERPLANE_COEFF;
    bm_l = inertia_l / displaced_vol;
}
```

The flag `r.hydro_table_used` is surfaced all the way to the console report:

```c
if (a.hydro_table_used)
    printf("Hydrostatics: Table interpolation\n");
else
    printf("Hydrostatics: Box-hull approximation\n");
```

So users always know which path was taken.

---

## When to trust the box-hull model

| Situation | Use box-hull? |
|---|---|
| Early planning, no table available | Yes — reliable order-of-magnitude result |
| Full-form cargo ship ($C_B \approx 0.70\text{–}0.80$) | Yes — constants were chosen for this hull type |
| Container ship, car carrier, or any unconventional form | Cautious use only — $C_B$ and $KB\_FACTOR$ may be wrong by 10–20% |
| Regulatory submission or class-society approval | No — always supply a measured hydrostatic table |
| Final voyage planning with known loading condition | No — use the table |

!!! warning
    The box-hull model uses fleet-average constants for a general cargo ship. If the vessel has an unusual hull form — a very high block coefficient (bulk carrier), a very low one (fast ferry), or a non-uniform cross-section — the computed draft, KB, and BM can be materially wrong. The model will not warn you; it will simply compute a result.

---

## Numeric example

Suppose a ship has $L = 150$ m, $B = 22$ m, and a total displacement of 8 000 t (lightship + cargo).

$$\nabla = \frac{8000}{1.025} \approx 7805 \text{ m}^3$$

$$T = \frac{7805}{150 \times 22 \times 0.75} \approx \frac{7805}{2475} \approx 3.15 \text{ m}$$

$$KB = 0.53 \times 3.15 \approx 1.67 \text{ m}$$

$$I_T = \frac{150 \times 22^3}{12} \times 0.85 = \frac{150 \times 10648}{12} \times 0.85 \approx 113\,160 \text{ m}^4$$

$$BM = \frac{113\,160}{7805} \approx 14.5 \text{ m}$$

If KG comes out at 5.0 m from cargo placement:

$$GM = 1.67 + 14.5 - 5.0 = 11.17 \text{ m}$$

That is a very large GM — the ship is very stiff. A 22 m beam drives that result because $B^3$ dominates. For a tall, heavily-loaded ship with a higher KG the margin would be smaller.

!!! tip
    BM is proportional to $B^2 / T$ (approximately). Beam is your biggest lever on transverse stability. Doubling beam roughly quadruples BM; doubling draft roughly halves BM.

---

## Recap

- CargoForge-C uses the box-hull model when no hydrostatic CSV table is loaded; the `hydro_table_used` flag tells you which path ran.
- Draft is computed from displaced volume divided by $L \times B \times C_B$, where $C_B = 0.75$ accounts for hull taper.
- KB is estimated as $0.53 \times T$, an empirical constant for cargo ships.
- BM is derived from the waterplane's second moment of area ($I_T = L B^3 / 12 \times C_W$) divided by displaced volume; breadth cubed makes beam the dominant term.
- GM = KB + BM − KG; all downstream stability criteria (GZ curve, IMO checks, heel) use the GM computed here after free-surface correction.
- The box-hull model is reliable for full-form general cargo ships at planning stage; for regulatory submissions or unusual hull forms, always supply a real hydrostatic table.

*Next: [The hydrostatic table](20-the-hydrostatic-table.md).*
