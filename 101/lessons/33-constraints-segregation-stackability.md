# Constraints: segregation and stackability

Placing a cargo item is not just a geometry problem — it is a safety problem. CargoForge-C
enforces six distinct rules for every candidate position before accepting it. Two of the most
consequential rules come from international law: the IMDG Code's segregation requirements for
dangerous goods, and structural limits on what cargo can be stacked below or above fragile and
refrigerated items. This lesson explains how those rules are encoded in [`src/constraints.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/constraints.c) and
[`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c), and why they are checked at placement time rather than as an afterthought.

## The mental model 🧠

Geometry decides where a box *can* go; constraints decide where it is *allowed* to go — and six rules can veto a spot that otherwise fits perfectly. **Segregation**: incompatible dangerous goods must stay a minimum distance apart — the oxidiser cannot sit beside the flammable, read straight from the IMDG matrix in `imdg.c`. **Stackability**: some cargo cannot take weight on top (fragile, refrigerated) and some cannot serve as a base. **Weight and stability**: even a legal stack is refused if it overloads a structure or pushes the centre of gravity too far.

In code this is a gate the placer must pass *before* it commits a position. For each candidate spot, `constraints.c` checks the segregation matrix against what is already nearby, checks the stack rules against what sits below and above, and checks the running weight. Fail any one and the spot is rejected and the search moves on. Checking at placement time, not as an afterthought, is the whole point: it is why a "valid" stow is a far smaller set than a "fits" stow — and why the program can certify a plan as *safe*, not merely compact.

<svg viewBox="0 0 600 196" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:580px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>IMDG segregation: incompatible dangerous goods need a minimum separation</title>
<desc>A flammable Class 3 item and an oxidizing Class 5.1 item must be kept apart. The IMDG segregation table maps the class pair to a segregation level, which sets a minimum separation distance. CargoForge-C rejects any placement that violates it.</desc>
<defs><marker id="dg-ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0 1 L9 5 L0 9 Z" fill="currentColor" opacity="0.8"/></marker></defs>
<text x="300" y="50" fill="currentColor" font-size="10.5" text-anchor="middle" opacity="0.6">IMDG Table 7.2.4 → segregation code → minimum distance</text>
<rect x="56" y="74" width="130" height="60" rx="5" fill="#D05663" fill-opacity="0.08" stroke="#D05663" stroke-width="1.3"/>
<text x="121" y="100" fill="#D05663" font-size="12.5" text-anchor="middle" font-weight="600">Class 3</text>
<text x="121" y="118" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">flammable liquid</text>
<rect x="414" y="74" width="130" height="60" rx="5" fill="#D05663" fill-opacity="0.08" stroke="#D05663" stroke-width="1.3"/>
<text x="479" y="100" fill="#D05663" font-size="12.5" text-anchor="middle" font-weight="600">Class 5.1</text>
<text x="479" y="118" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.7">oxidizer</text>
<line x1="192" y1="104" x2="408" y2="104" stroke="currentColor" stroke-width="1.3" marker-start="url(#dg-ar)" marker-end="url(#dg-ar)" opacity="0.8"/>
<text x="300" y="98" fill="currentColor" font-size="13" text-anchor="middle" font-weight="600">≥ 6 m</text>
<text x="300" y="120" fill="currentColor" font-size="10" text-anchor="middle" opacity="0.6">“separated from”</text>
<text x="300" y="172" fill="currentColor" font-size="11" text-anchor="middle" opacity="0.65"><tspan font-family="var(--md-code-font,monospace)">src/imdg.c</tspan> rejects any placement that violates the required separation.</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **IMDG segregation** = "international law that says which dangerous goods must be kept apart — and by how much" — `imdg_get_segregation` in `src/imdg.c` looks up the pair of hazard classes in a 17 × 17 table and returns a code (none, 3 m, 6 m, 12 m, 24 m, or "never on the same ship").
- **`check_cargo_constraints`** = "the bouncer that approves or blocks every candidate position before anything is written" — called inside `find_best_fit_3d` for every bin × space × orientation; returning 0 means "skip this spot," so a violation can never be *introduced* by the optimizer.
- **Point load** = "weight per square metre of floor" — a small, dense item can punch through decking even if the ship's total weight limit is fine; `calculate_point_load` divides the cargo's weight (converted from kg to tonnes) by its footprint and rejects anything over 5 t/m².
- **Stacking pressure** = "how much weight the items above are pressing down on whatever you want to place here" — `calculate_stack_pressure` uses axis-aligned bounding box overlap fractions so partial overlaps count proportionally, not as all-or-nothing; fragile cargo gets a tighter ceiling than general cargo.
- **`SEG_INCOMPATIBLE`** = "these two classes of dangerous goods cannot share the same vessel — full stop" — it maps to −1 in `imdg_min_distance`, and `check_cargo_constraints` treats it as an immediate hard rejection without even computing a distance.
- **Advisory vs. hard reject** = "some rules are legal limits (reject the spot), others are operational preferences (warn the operator but allow it)" — reefer cargo outside the Deck bin and fragile cargo below −5 m both generate stderr notes but do not block placement, because the ship still needs to sail even when the deck is full.
- **`imdg_check_all`** = "a second, independent O(n²) sweep run after all placement is done" — it uses edge-to-edge horizontal distance (not centre-to-centre) to catch any pair that slipped through, and collects up to `MAX_IMDG_VIOLATIONS` entries into an `IMDGCheckResult` that library consumers can inspect.

**Why it matters:** if the constraint check ran *after* placement rather than *during* it, the optimizer could commit to a layout that violates IMDG law or punches through decking — and unwinding that after the fact could require replanning the entire stow. Getting it wrong at sea means fire, explosion, or structural failure with no easy fix.

---

## Why constraints live in the placement loop

The bin-packing algorithm in `placement_3d.c` evaluates every candidate space before committing
to it. The call to `check_cargo_constraints` is the gatekeeper inside `find_best_fit_3d`: if it
returns 0, that space is skipped and the search continues. This means a dangerous-goods violation
or a structural overload can never be *introduced* by the optimizer — the constraint is applied
before any coordinate is written to the `Cargo` struct.

```
find_best_fit_3d(...)
    for each bin × space × orientation:
        if fits_in_space(...) && check_cargo_constraints(...):
            record as best candidate
```

The six checks run in a fixed order from cheapest to most expensive. The first failing check
short-circuits the rest.

---

## The six constraint checks

### 1. Point load

The floor of every hold has a structural limit, typically expressed in tonnes per square metre
(t/m²). A small but very heavy item concentrates all of its weight onto a tiny footprint and can
punch through decking.

From [`src/constraints.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/constraints.c):

```c
float calculate_point_load(const Cargo *cargo) {
    float area = cargo->dimensions[0] * cargo->dimensions[1];
    if (area < 0.01f) return 0.0f;
    return (cargo->weight / 1000.0f) / area;  /* t/m2 */
}
```

`cargo->weight` is stored in kilograms, so dividing by 1000 converts to tonnes before dividing
by footprint area (length × width in m²). The result is compared against `MAX_POINT_LOAD`
(5.0 t/m²). Any cargo exceeding this is rejected regardless of which space it occupies.

### 2. IMDG segregation (dangerous goods)

This is the most rule-dense check. When the cargo being placed carries a `dg` pointer (meaning
its manifest entry had a valid `DG:` field), `check_cargo_constraints` runs a full IMDG
segregation check against every already-placed DG cargo item:

```c
SegregationType req = imdg_get_segregation(
    cargo->dg->dg_class, cargo->dg->dg_division,
    c->dg->dg_class, c->dg->dg_division);

if (req == SEG_INCOMPATIBLE) {
    fprintf(stderr, "Constraint: %s incompatible with %s (IMDG)\n",
            cargo->id, c->id);
    return 0;
}

float min_dist = imdg_min_distance(req);
if (min_dist > 0.0f) {
    float dx = c->pos_x - space->x;
    float dy = c->pos_y - space->y;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < min_dist) { return 0; }
}
```

If the cargo has `type = "hazardous"` but no `dg` pointer (i.e., no `DG:` field in the
manifest), the legacy 3 m Euclidean-distance fallback `check_hazmat_separation` is used instead.

### 3. Stacking pressure

Every item placed above a candidate position adds weight to whatever is below. The function
`calculate_stack_pressure` accumulates that weight using axis-aligned bounding box (AABB)
overlap to figure out what fraction of each already-placed item's weight lands on the candidate
footprint:

```c
float overlap_frac = (overlap_x * overlap_y) / c_area;
total_weight_above += (c->weight / 1000.0f) * overlap_frac;
```

The pressure is `total_weight_above / footprint` in t/m². Fragile cargo uses a tighter limit
(`MAX_STACK_PRESSURE_FRAGILE`); everything else uses `MAX_STACK_PRESSURE` (10 t/m²).

### 4. Reefer preference (advisory)

Refrigerated cargo needs access to ship's power and ventilation, both of which are available on
deck in modern container ships. If a reefer item is placed in a hold rather than the `"Deck"`
bin, the code emits a warning to stderr but does **not** reject the placement:

```c
if (is_reefer(cargo) && strcmp(bin->name, "Deck") != 0) {
    fprintf(stderr, "Note: Reefer %s placed in %s (deck preferred)\n", ...);
}
```

This is intentional: a reefer that fits nowhere on deck must still go somewhere rather than be
left unplaced.

### 5. Fragile depth advisory

Similarly, fragile cargo placed deep in a hold (z < −5.0 m below the origin) generates an
advisory. A deep position makes inspection and unloading harder. Like the reefer check, this
does not cause a rejection.

### 6. Deck weight ratio

The `"Deck"` bin has a hard structural limit. If placing the item would push the deck's
accumulated weight past `MAX_DECK_WEIGHT_RATIO` of `ship->max_weight`, the space is rejected:

```c
float deck_weight_ratio =
    (bin->current_weight + cargo->weight) / ship->max_weight;
if (deck_weight_ratio > MAX_DECK_WEIGHT_RATIO) { return 0; }
```

---

## The IMDG segregation matrix

The International Maritime Dangerous Goods (IMDG) Code classifies all hazardous substances into
nine primary classes and numerous divisions. Two different dangerous goods in proximity may be
harmless, incompatible, or somewhere in between. The IMDG Code encodes this as a segregation
table (Table 7.2.4 in Amendment 41-22): a matrix of requirements for every class-pair.

CargoForge-C encodes that table as a static 17 × 17 integer array in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c):

```c
static const int seg_matrix[MATRIX_SIZE][MATRIX_SIZE] = {
/*             1.1-6  1.7  1.8  2.1  2.2  2.3   3   4.1  4.2  4.3  5.1  5.2  6.1  6.2   7    8    9  */
/* 1.1-6 */ {  5,     5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5  },
/* 1.7   */ {  5,     0,   0,   2,   1,   2,   2,   1,   2,   2,   2,   2,   1,   0,   2,   1,   0  },
/* 2.1   */ {  5,     2,   1,   0,   0,   0,   2,   1,   2,   0,   2,   2,   0,   0,   2,   1,   0  },
/* 3     */ {  5,     2,   1,   2,   1,   2,   0,   0,   2,   1,   2,   2,   0,   0,   2,   1,   0  },
/* 5.1   */ {  5,     2,   1,   2,   1,   2,   2,   1,   2,   2,   0,   2,   1,   0,   1,   2,   0  },
/* ...                                                                                               */
};
```

The matrix has 17 rows and columns, not 9, because classes with important sub-divisions each get
their own row. The mapping from IMDG class and division to a matrix index is handled by
`class_to_index` in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c).

### The 17 row/column indices

| Index | IMDG class/division | Description |
|-------|--------------------|----|
| 0 | 1.1 – 1.6 | Explosives (all divisions except 1.7 and 1.8) |
| 1 | 1.7 | Explosives, CO-compatible |
| 2 | 1.8 | Desensitized explosives |
| 3 | 2.1 | Flammable gases |
| 4 | 2.2 | Non-flammable, non-toxic gases |
| 5 | 2.3 | Toxic gases |
| 6 | 3   | Flammable liquids |
| 7 | 4.1 | Flammable solids |
| 8 | 4.2 | Spontaneously combustible |
| 9 | 4.3 | Dangerous when wet |
| 10 | 5.1 | Oxidizing substances |
| 11 | 5.2 | Organic peroxides |
| 12 | 6.1 | Toxic substances |
| 13 | 6.2 | Infectious substances |
| 14 | 7   | Radioactive material |
| 15 | 8   | Corrosive substances |
| 16 | 9   | Miscellaneous dangerous substances |

### Matrix values and their meaning

Each cell holds an integer code that maps to a `SegregationType` enum:

| Cell value | Enum | Minimum separation |
|------|------|----|
| 0 | `SEG_NONE` | None required |
| 1 | `SEG_AWAY_FROM` | 3 m |
| 2 | `SEG_SEPARATED` | 6 m |
| 3 | `SEG_SEPARATED_COMPLETE` | 12 m |
| 4 | `SEG_SEPARATED_LONG` | 24 m |
| 5 | `SEG_INCOMPATIBLE` | Cannot share same vessel |

These distances come directly from `imdg_min_distance` in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c):

```c
float imdg_min_distance(SegregationType seg) {
    switch (seg) {
        case SEG_NONE:               return 0.0f;
        case SEG_AWAY_FROM:          return 3.0f;
        case SEG_SEPARATED:          return 6.0f;
        case SEG_SEPARATED_COMPLETE: return 12.0f;
        case SEG_SEPARATED_LONG:     return 24.0f;
        case SEG_INCOMPATIBLE:       return -1.0f;
        default:                     return 0.0f;
    }
}
```

Notice that `SEG_INCOMPATIBLE` returns −1. The caller in `check_cargo_constraints` treats any
`SEG_INCOMPATIBLE` result as an immediate hard rejection — it never even checks the distance.

### Reading a concrete example

Suppose a manifest contains a flammable liquid (Class 3, `DG:3:UN1203:...`) and an oxidizing
substance (Class 5.1, `DG:5.1:UN1942:...`). Looking up row 6 (Class 3) and column 10 (Class
5.1) in `seg_matrix` gives the value `2` = `SEG_SEPARATED` → **minimum 6 m edge-to-edge
horizontal distance**.

```c
SegregationType req = imdg_get_segregation(3, 0, 5, 1);
// returns SEG_SEPARATED
float min_dist = imdg_min_distance(req);
// returns 6.0f
```

If the optimizer proposes placing the flammable liquid only 4 m from the already-placed
oxidizer, `check_cargo_constraints` rejects that space and tries the next candidate.

!!! note "Horizontal distance, not 3D"
    The IMDG Code specifies horizontal separation. `check_cargo_constraints` computes
    `sqrtf(dx*dx + dy*dy)` — X and Y only. Vertical stacking of incompatible goods is
    governed separately by stowage categories (`STOW_ON_DECK`, `STOW_UNDER_DECK`) encoded
    in `DGInfo.stowage`, not by this distance check.

---

## Post-placement verification: `imdg_check_all`

The constraint checks during placement are eager — they prevent violations from being
introduced. But `imdg_check_all` in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c) provides a second, independent O(n²) sweep
run *after* placement is complete (via `cargoforge_check_imdg` in the public API). It checks
every placed DG pair using edge-to-edge horizontal distance:

```c
static float cargo_horizontal_distance(const Cargo *a, const Cargo *b) {
    float ax = a->pos_x + a->dimensions[0] / 2.0f;
    float ay = a->pos_y + a->dimensions[1] / 2.0f;
    float bx = b->pos_x + b->dimensions[0] / 2.0f;
    float by = b->pos_y + b->dimensions[1] / 2.0f;

    float dx = fabsf(ax - bx) - (a->dimensions[0] + b->dimensions[0]) / 2.0f;
    float dy = fabsf(ay - by) - (a->dimensions[1] + b->dimensions[1]) / 2.0f;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    return sqrtf(dx * dx + dy * dy);
}
```

This computes the edge-to-edge distance by subtracting half the combined widths from the
center-to-center distance. Two items whose bounding boxes touch have distance 0.

Violations are collected into an `IMDGCheckResult` with up to `MAX_IMDG_VIOLATIONS` entries,
each including the cargo indices, required segregation type, actual distance, and a
human-readable description. The `cargoforge_imdg_violation` API function exposes individual
entries to library consumers.

!!! warning "Two separate IMDG paths"
    During placement, `check_cargo_constraints` uses the candidate position (`space->x`,
    `space->y`) — the item is not yet written to the `Cargo` struct. After placement,
    `imdg_check_all` reads from `cargo->pos_x` / `cargo->pos_y`. These are consistent for
    items that were successfully placed, but it is important to understand that the
    pre-placement check uses the prospective position, not a committed one.

---

## Reefer and fragile: advisory vs. hard rejection

It is worth being explicit about the asymmetry between the six constraint checks:

| Check | Hard reject? |
|-------|-------------|
| Point load > 5 t/m² | Yes |
| IMDG incompatible pair | Yes |
| IMDG distance too small | Yes |
| Stacking pressure too high | Yes |
| Reefer outside Deck | No (advisory) |
| Fragile below −5 m | No (advisory) |
| Deck weight ratio exceeded | Yes |

Reefer and fragile generate stderr notes, not rejection codes. This is deliberate: a ship
carrying 200 reefer containers cannot leave half of them on the dock because the deck is full.
The warnings inform the operator; the operator decides whether to accept the compromise or
reconfigure the stow.

---

## Recap

- `check_cargo_constraints` in [`src/constraints.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/constraints.c) is called for every candidate space in the
  placement loop; it returns 0 to reject and 1 to accept.
- Six checks run in order: point load, IMDG/hazmat separation, stacking pressure, reefer
  preference, fragile depth, deck weight ratio — four are hard rejects, two are advisory.
- The IMDG segregation engine in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c) encodes IMDG Code Table 7.2.4 as a 17 × 17
  static integer matrix covering all 17 class/division groupings.
- `imdg_min_distance` translates segregation codes to required metres: 0, 3, 6, 12, 24, or −1
  (incompatible).
- `imdg_check_all` provides a post-placement O(n²) sweep using edge-to-edge horizontal
  distance; it is separate from the per-placement check.
- Stacking pressure uses AABB overlap fractions so partial overlaps are counted proportionally,
  not as all-or-nothing.

*Next: [Lab 8 - Place cargo and trigger a violation](lab-08-placement.md).*
