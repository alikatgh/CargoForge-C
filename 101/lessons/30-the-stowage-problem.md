# The stowage problem

Fitting cargo into ship holds sounds mechanical — a matter of measuring boxes and doing arithmetic. In practice, it belongs to a family of combinatorial problems that have resisted exact polynomial-time solutions for decades. This lesson explains what bin packing is, why it is computationally hard, and how CargoForge-C trades optimality for speed with a set of practical heuristics.

## The mental model 🧠

Stowage is Tetris where you never see the next piece and you are not allowed to lose. You have a fixed set of holds and a pile of boxes of every size, and you must fit them in with nothing overlapping, no hold over its weight cap, and the heavy items kept low and centred. `place_cargo_3d` is the engine that attempts it.

The trap is that the number of arrangements explodes. Every box can go in many positions, in any of six orientations, in any order — so for even a few dozen items, trying them all is hopeless. That is what *NP-hard* means: no known shortcut, and brute force that blows up faster than any computer can chase. So CargoForge never searches for the perfect plan; it makes fast, sensible greedy choices (Lesson 31) and produces a valid, stable stow in milliseconds. When a piece genuinely has no home it is stamped with the `pos_x = -1` sentinel and left out of the stability sums rather than forced. "Optimal" is off the table; "safe and quick" is the whole game.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The stowage problem: pack varied boxes into holds without overlap or overload</title>
<desc>A pile of cargo boxes of different sizes must be packed into the ship's holds without overlapping, without exceeding weight limits, and keeping the load stable. The number of possible arrangements grows so fast that finding the perfect one is infeasible (NP-hard), so CargoForge uses a fast heuristic instead.</desc>
<text x="92" y="40" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.7">cargo</text>
<g stroke="#12A594" stroke-width="1.1" fill="#12A594" fill-opacity="0.1">
<rect x="40" y="54" width="64" height="40" rx="2"/>
<rect x="112" y="64" width="40" height="30" rx="2"/>
<rect x="40" y="104" width="44" height="34" rx="2"/>
<rect x="92" y="104" width="60" height="50" rx="2"/>
<rect x="40" y="146" width="36" height="26" rx="2"/>
</g>
<line x1="166" y1="110" x2="206" y2="110" stroke="currentColor" stroke-opacity="0.5"/><path d="M199,106 L206,110 L199,114" fill="none" stroke="currentColor" stroke-opacity="0.6"/>
<rect x="214" y="42" width="240" height="148" rx="4" fill="none" stroke="currentColor" stroke-width="1.3" stroke-opacity="0.6"/>
<text x="334" y="36" font-size="10.5" text-anchor="middle" fill="currentColor" opacity="0.7">hold</text>
<g stroke="#12A594" stroke-width="1" fill="#12A594" fill-opacity="0.12">
<rect x="220" y="126" width="80" height="58"/>
<rect x="304" y="146" width="56" height="38"/>
<rect x="364" y="100" width="50" height="84"/>
<rect x="220" y="86" width="40" height="36"/>
<rect x="264" y="92" width="36" height="30"/>
</g>
<rect x="304" y="100" width="56" height="42" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.25" stroke-dasharray="3 3"/>
<text x="332" y="125" font-size="8" text-anchor="middle" fill="currentColor" opacity="0.45">gap</text>
<rect x="418" y="100" width="30" height="84" fill="currentColor" fill-opacity="0.03" stroke="currentColor" stroke-opacity="0.25" stroke-dasharray="3 3"/>
<text x="528" y="116" font-size="9.5" text-anchor="middle" fill="#D05663" opacity="0.85"><tspan x="528" dy="0">arrangements</tspan><tspan x="528" dy="13">explode</tspan><tspan x="528" dy="13">(NP-hard)</tspan></text>
<text x="300" y="210" font-size="10" text-anchor="middle" fill="currentColor" opacity="0.7">can't try them all → a fast heuristic (Lesson 31), not a perfect search</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **3D bin packing** = "figure out which box goes where in which hold" — the geometric core of what `place_cargo_3d` solves: assign every `Cargo` item a position inside one of the ship's rectangular `Bin3D` holds without anything overlapping or sticking out.
- **NP-hard** = "no one knows a shortcut; brute-force search explodes too fast to use" — with 50 items across 3 holds the number of possible arrangements is astronomical, which is why CargoForge-C never tries to find the perfect answer and uses fast approximations instead.
- **First Fit Decreasing (FFD)** = "load the biggest crates first" — `place_cargo_3d` sorts `ship->cargo` by volume (largest to smallest) before placing anything, because large items are hardest to fit and small ones can fill the gaps that remain.
- **Guillotine splitting** = "after placing a box, slice the leftover space into up to three clean rectangles" — `split_space_3d` records a right-remainder, a back-remainder, and a top-remainder in `bin->spaces[]` so future items have somewhere to land; the downside is that many placements leave small slivers that together might hold an item but individually do not.
- **Six orientations** = "try turning every crate every which way before giving up" — `get_orientation_dims` enumerates all 3! permutations of length, width, and height, so a crate that does not fit standing up is also tried on its side and on its end.
- **Sentinel position (`pos_x = -1.0f`)** = "this item has no home yet" — when `find_best_fit_3d` finds no viable space, it stamps the cargo with `-1`; `perform_analysis` then skips it entirely when computing KG, trim, and heel, so an unplaced item cannot silently corrupt the stability numbers.
- **Constraint gate before geometry** = "a space that breaks a safety rule does not exist" — `check_cargo_constraints` enforces point-load limits, IMDG segregation distances, and stacking pressure before the volume comparison even runs; a space that fails any check is treated as if it were already full.

**Why it matters:** a placement that looks geometrically perfect can still capsize a ship if heavy cargo ends up all on one side, or kill a dockworker if incompatible DG classes end up adjacent — which is exactly why `place_cargo_3d` feeds its results into `perform_analysis` and runs `check_cargo_constraints` on every candidate space before accepting it.

---

## What we are actually trying to solve

A cargo ship has a finite set of holds, each with fixed dimensions and a weight ceiling. The manifest lists cargo items, each with its own dimensions, weight, and constraints (fragile, hazardous, refrigerated…). We want to assign every item to a position in a hold so that:

- Nothing sticks out of the boundaries of its hold.
- The total weight in each hold stays below its limit.
- Safety rules are satisfied (no hazmat beside fragile glass, adequate separation between incompatible DG classes, and so on).
- After placement, the resulting center of gravity keeps the ship stable.

The core geometric sub-problem — pack a set of rectangular boxes into a set of rectangular containers — is the **3D bin-packing problem**.

!!! note "Why geometry alone is not enough"
    A pure geometric packer ignores weight distribution. CargoForge-C must also feed the final positions into `perform_analysis` so that KG, GM, trim, and heel can be computed. A perfectly tight packing that puts all heavy cargo on one side produces a dangerous heel angle. Placement and stability analysis are inseparable.

---

## Why bin packing is NP-hard

NP-hardness means, informally, that no algorithm is known that can guarantee an optimal solution and also run in time proportional to a polynomial function of the input size. As the number of items grows, the search space explodes.

To see why, consider even the simplest 1D version: the **subset-sum problem**. Given a set of weights and a target capacity, decide whether some subset fills the bin exactly. That decision problem is NP-complete. 3D bin packing contains subset-sum as a special case (set all heights and depths to 1, and each "bin" becomes a 1D knapsack), so it is at least as hard.

The combinatorial explosion for 3D packing is severe:

- With $n$ items and $b$ bins, the number of ways to assign items to bins is $b^n$.
- Each item can be placed in 6 axis-aligned orientations (all permutations of length, width, height).
- Each position in a bin can be any valid coordinate within the remaining free space.

For a modest manifest of 50 items across 3 holds, the search space is already astronomical. Enumerating it is not practical on any hardware that will exist in the foreseeable future.

---

## CargoForge-C's answer: FFD + guillotine splitting

CargoForge-C uses two well-studied heuristics in combination.

### First Fit Decreasing (FFD)

**Idea:** sort items from largest to smallest before placing anything. Large items are the hardest to fit; placing them first leaves smaller gaps that can accommodate smaller items. Small items are flexible and can usually be squeezed in somewhere.

From [`src/placement_3d.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/placement_3d.c), the comparator and the sort:

```c
// Comparator for sorting cargo by volume (descending)
static int cargo_cmp_by_volume_desc(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a;
    const Cargo *cb = (const Cargo *)b;
    float vol_a = ca->dimensions[0] * ca->dimensions[1] * ca->dimensions[2];
    float vol_b = cb->dimensions[0] * cb->dimensions[1] * cb->dimensions[2];
    if (vol_a < vol_b) return 1;
    if (vol_a > vol_b) return -1;
    return 0;
}
```

```c
void place_cargo_3d(Ship *ship) {
    // Sort cargo by volume (largest first - FFD heuristic)
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_volume_desc);
    ...
```

`qsort` is the C standard library's generic sort. It receives the comparator above and reorders `ship->cargo` in place.

### Best-fit within free spaces

After sorting, each item is offered to the available free rectangles across all bins. The algorithm picks the **tightest-fitting free space** — the one whose volume is smallest while still large enough for the item. This minimizes wasted space per placement.

```c
float best_fit_volume = 1e9f;  // Find tightest fit (minimize wasted space)

for (int b = 0; b < bin_count; b++) {
    ...
    for (int s = 0; s < bin->space_count; s++) {
        ...
        for (int o = 0; o < 6; o++) {
            if (fits_in_space(space, cargo, o)) {
                float vol = space_volume(space);
                // Prefer smaller spaces (tighter fit = less waste)
                if (vol < best_fit_volume) {
                    best_fit_volume = vol;
                    *best_bin = b;
                    *best_space = s;
                    *best_orientation = o;
                }
            }
        }
    }
}
```

The `o` loop runs all six orientations for each candidate space. This matters in practice: a 6 m × 2 m × 2.5 m crate and a 4 m × 4 m × 2 m gap may not fit in the default orientation but will fit if the crate is rotated.

### Six orientations

`get_orientation_dims` enumerates all permutations of the three dimensions:

```c
switch (orientation) {
    case 0: *w = dims[0]; *d = dims[1]; *h = dims[2]; break; // XYZ
    case 1: *w = dims[0]; *d = dims[2]; *h = dims[1]; break; // XZY
    case 2: *w = dims[1]; *d = dims[0]; *h = dims[2]; break; // YXZ
    case 3: *w = dims[1]; *d = dims[2]; *h = dims[0]; break; // YZX
    case 4: *w = dims[2]; *d = dims[0]; *h = dims[1]; break; // ZXY
    case 5: *w = dims[2]; *d = dims[1]; *h = dims[0]; break; // ZYX
```

Six = 3! (three-factorial), covering all axis-aligned rotations. For items that cannot be rotated in practice (e.g. a standing tank), the caller could restrict which orientation indices are valid — but the current implementation always tries all six.

---

## How free space is tracked: guillotine splitting

After placing an item, how does the algorithm know what space remains in the bin? CargoForge-C uses the **guillotine algorithm**: a single straight cut splits the remaining volume into rectangular pieces, like cutting a piece of paper.

`split_space_3d` implements this. When a cargo item occupies part of a free space, the original space is marked occupied and up to three new free spaces are recorded — the remainder to the right, the remainder behind, and the remainder above:

```c
// Mark original space as occupied
original->is_free = 0;

// Right remainder (along width)
if (original->width > cargo_w) {
    Space3D right = {
        .x = original->x + cargo_w,
        .y = original->y,
        .z = original->z,
        .width = original->width - cargo_w,
        .depth = original->depth,
        .height = original->height,
        .is_free = 1
    };
    bin->spaces[bin->space_count++] = right;
}

// Back remainder (along depth)
if (original->depth > cargo_d) {
    Space3D back = {
        .x = original->x,
        .y = original->y + cargo_d,
        .z = original->z,
        .width = cargo_w,
        .depth = original->depth - cargo_d,
        .height = original->height,
        .is_free = 1
    };
    bin->spaces[bin->space_count++] = back;
}

// Top remainder (along height)
if (original->height > cargo_h) {
    Space3D top = {
        .x = original->x,
        .y = original->y,
        .z = original->z + cargo_h,
        .width = cargo_w,
        .depth = cargo_d,
        .height = original->height - cargo_h,
        .is_free = 1
    };
    bin->spaces[bin->space_count++] = top;
}
```

Each new space is appended to `bin->spaces[]`. The array has a maximum of `MAX_FREE_RECTS = 1024` entries; `split_space_3d` refuses to create new spaces if fewer than 3 slots remain, printing a warning and leaving the bin unmodified for that placement.

!!! warning "Guillotine waste"
    Guillotine splitting is simple and fast, but it produces fragments. After many placements, a bin may contain hundreds of small free rectangles, none large enough for the remaining cargo — even if the total free volume would theoretically fit it. This is the principal source of suboptimality in CargoForge-C's packer. More advanced algorithms (Maximal Rectangles, Skyline) keep less fragmentation at the cost of greater complexity.

---

## The three bins

`place_cargo_3d` creates three hard-coded bins proportioned from the ship's dimensions:

| Bin | Position | Size | Max weight |
|---|---|---|---|
| `ForwardHold` | Bow 30% of length | L × 0.30, B × 0.80, 8 m deep | 30% of ship max |
| `AftHold` | Stern 30% of length | L × 0.30, B × 0.80, 8 m deep | 30% of ship max |
| `Deck` | Full length | L, B, 4 m high | 40% of ship max |

Holds start at z = -8 m (below waterline) and are 80% of beam width to leave room for side tanks. Deck starts at z = 0. The central 40% of ship length between the two holds is intentionally left without a dedicated hold — large vessels would have additional holds there, but the three-bin model captures the primary structural zones and avoids over-engineering the heuristic for a teaching codebase.

```c
// Forward hold (30% of length)
bins[0] = (Bin3D){
    .name = "ForwardHold",
    .x = 0.0f, .y = 0.0f, .z = -8.0f,
    .width = ship->length * 0.3f,
    .depth = ship->width * 0.8f,
    .height = 8.0f,
    .max_weight = ship->max_weight * 0.3f,
    ...
};
```

---

## Constraints woven into placement

Pure geometry is not enough. Before accepting any candidate placement, `find_best_fit_3d` calls `check_cargo_constraints`, which enforces:

1. **Point load** — weight / footprint must not exceed 5 t/m². Heavy concentrated cargo crushes deck plates.
2. **IMDG segregation** — if both items carry DG metadata, the IMDG matrix is consulted. Incompatible pairs are hard-rejected; pairs requiring separation enforce a minimum horizontal distance.
3. **Legacy hazmat** — type=`hazardous` items without full DG data still require 3 m clearance from each other.
4. **Stacking pressure** — the weight of already-placed cargo above the candidate position is summed. Fragile cargo uses a tighter pressure limit.
5. **Reefer preference** — reefer cargo placed outside the Deck zone produces an advisory warning but is not rejected (refrigerated containers can be placed in holds with plug-in power).
6. **Deck weight ratio** — if adding this item to the Deck would exceed the deck weight fraction limit, the space is skipped.

Constraints are checked before the volume comparison. A space that passes all constraints but is not the tightest fit is still considered. A space that fails any constraint is skipped entirely, as if it did not exist.

---

## What happens to items that do not fit

If `find_best_fit_3d` finds no viable placement, the item's position is set to the sentinel value:

```c
c->pos_x = c->pos_y = c->pos_z = -1.0f;
```

`perform_analysis` in `analysis.c` skips these items when computing KG, trim, and heel — it checks `pos_x >= 0` before including a cargo item in any moment calculation. The loading report will note unplaced items, and the operator must intervene manually.

---

## Why not use an exact algorithm?

The only known algorithms that always find the optimal 3D bin-packing solution use exhaustive or branch-and-bound search. Their worst-case runtime is exponential in the number of items. A 30-item manifest might have a million candidate orderings worth exploring; a 200-item manifest makes exhaustive search impractical even on a server cluster.

The FFD + best-fit + guillotine heuristic makes decisions greedily — each item is placed once and never reconsidered. It runs in $O(n^2 \cdot m \cdot s)$ time where $n$ is the number of items, $m$ is the total number of bins, and $s$ is the maximum number of free spaces per bin. For realistic manifests this is fast: a 100-item load completes in milliseconds.

The trade-off is that the heuristic can fail to find a valid arrangement even when one exists, particularly when later-sorted (smaller) items fragment the space needed by a mid-sized item. In practice, for the class of manifests CargoForge-C targets, FFD produces placements close to optimal.

!!! tip "Approximation guarantees"
    For the 1D bin-packing problem, FFD is known to use at most $\frac{11}{9} \cdot OPT + \frac{6}{9}$ bins — never more than about 22% worse than optimal. No tight bound is established for 3D in the general case, but empirically it performs well on structured cargo where items have similar proportions.

---

## Recap

- Fitting boxes into holds is an instance of **3D bin-packing**, a problem proven NP-hard — no polynomial-time exact algorithm is known.
- CargoForge-C uses **First Fit Decreasing** (sort large items first) combined with **best-fit selection** (choose the tightest-fitting free space) and **guillotine splitting** (split leftovers into three rectangular remainders).
- All six axis-aligned orientations are tried for every candidate space so that items can be rotated to fit.
- Three hard-coded bins — `ForwardHold`, `AftHold`, and `Deck` — partition the ship proportionally by length and weight capacity.
- **Constraint checking** (`check_cargo_constraints`) gates every candidate placement: point load, IMDG segregation, stacking pressure, and deck limits are enforced before a space is accepted.
- Items that cannot be placed are marked with `pos_x = -1.0f` and excluded from stability calculations.

*Next: [First-Fit-Decreasing](31-first-fit-decreasing.md).*
