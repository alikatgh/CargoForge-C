# First-Fit-Decreasing

Cargo does not arrange itself. Given a manifest of containers, machinery, and bulk bags, someone — or some algorithm — must decide which item goes where. CargoForge-C solves this with a classical combinatorial heuristic called **First-Fit Decreasing** (FFD), implemented in [`src/placement_3d.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/placement_3d.c). This lesson walks through why FFD exists, how the code applies it in three dimensions, and why the sort order matters for stability as much as it matters for packing efficiency.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **First-Fit Decreasing (FFD)** = "sort biggest items first, then slot each one into the first place it fits" — CargoForge-C uses this as the top-level strategy in `place_cargo_3d`, sorting the entire `ship->cargo` array with `qsort` before touching a single bin.
- **Bin** = "one named stowage zone with its own size and weight budget" — the code creates three concrete bins — `ForwardHold`, `AftHold`, and `Deck` — each modelled as a 3D box with a maximum weight; cargo must satisfy both a geometric fit and a weight check to land in a bin.
- **`cargo_cmp_by_volume_desc`** = "a referee function that tells `qsort` which cargo item is bigger" — it multiplies each `Cargo`'s three `dimensions[]` values to get cubic metres and returns descending order, so the heaviest-footprint items go first and are never squeezed out by earlier smaller items.
- **Six orientations** = "try rotating the box every which way before giving up" — `find_best_fit_3d` calls `get_orientation_dims` for all six permutations of length/width/height, so a container that doesn't fit standing up may still fit on its side.
- **Tightest-fit selection** = "prefer the space that wastes the least room" — among all valid bin/space/orientation combinations, `find_best_fit_3d` keeps the one with the smallest free-space volume, which is why the lesson calls it a Best-Fit variant of FFD rather than pure FFD.
- **`pos_x = -1.0f` sentinel** = "this item has no home yet" — when no bin can accept a cargo piece, `place_cargo_3d` marks it with negative coordinates and prints a warning; `perform_analysis` then skips it entirely so an unplaceable item never corrupts the stability numbers.
- **Sort order as a stability policy** = "large items land in the holds, which keeps the ship's centre of gravity low" — because holds are tried before the deck and have higher headroom, the FFD sort naturally drives heavy cargo down to where it lowers KG, raises GM, and keeps the ship within the IMO 0.15 m minimum.

**Why it matters:** if you sort small items first, the holds fragment early and large machinery ends up on deck, raising KG and potentially pushing GM below the safety limit — a packing algorithm that ignores sort order is also a hidden stability risk.

---

## The mental model 🧠

You'll forget the formula — hold THIS picture instead:

> You're packing a moving truck. You load the sofa first, then the wardrobe, then the armchairs — always the biggest piece first. By the time you're down to boxes and bags, every awkward gap has a shape that a small box can fill perfectly. If you'd started with the boxes, you'd have a truck full of air pockets and no room for the sofa.

That's FFD. In CargoForge-C, the "sofa" is a 10 m × 4 m machine that must land in `ForwardHold` or `AftHold` before anything else fragments those spaces. `cargo_cmp_by_volume_desc` is the rule that keeps the sofa at the front of the queue; `qsort` acts on it before `find_best_fit_3d` is called even once.

The stability bonus is built into the same picture: heavy furniture rides on the floor of the truck (low KG), not on top of stacked boxes (high KG). Because the holds are tried before the deck and have more headroom, FFD's sort naturally drives heavy cargo down — raising GM and keeping the ship inside the IMO 0.15 m floor without any separate stability pass.

---

<svg viewBox="0 0 620 280" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>First-Fit Decreasing placement pipeline</title>
  <desc>Three-stage diagram: (1) cargo array sorted largest-first by cargo_cmp_by_volume_desc, (2) find_best_fit_3d tries ForwardHold then AftHold then Deck across six orientations, picking the tightest fit, (3) placed items get real coordinates while unplaced items get the pos_x=-1 sentinel.</desc>

  <!-- Stage labels background strips -->
  <rect x="10" y="10" width="180" height="260" rx="6" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.18" stroke-width="1"/>
  <rect x="210" y="10" width="200" height="260" rx="6" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.18" stroke-width="1"/>
  <rect x="430" y="10" width="180" height="260" rx="6" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.18" stroke-width="1"/>

  <!-- Stage 1: Sort -->
  <text x="100" y="36" text-anchor="middle" font-size="12" font-weight="700" fill="currentColor" fill-opacity="0.9">① Sort</text>
  <text x="100" y="52" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.6">cargo_cmp_by_volume_desc</text>

  <!-- Cargo boxes — large to small, largest is teal -->
  <rect x="28" y="66" width="144" height="34" rx="4" fill="#12A594" fill-opacity="0.85"/>
  <text x="100" y="87" text-anchor="middle" font-size="11" font-weight="600" fill="white">Machine  120 m³</text>

  <rect x="28" y="108" width="144" height="30" rx="4" fill="currentColor" fill-opacity="0.14"/>
  <text x="100" y="128" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.85">Container  72 m³</text>

  <rect x="28" y="146" width="144" height="26" rx="4" fill="currentColor" fill-opacity="0.1"/>
  <text x="100" y="164" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.75">Pallet  18 m³</text>

  <rect x="28" y="180" width="144" height="20" rx="4" fill="currentColor" fill-opacity="0.07"/>
  <text x="100" y="195" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.6">Box  4 m³</text>

  <text x="100" y="228" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">qsort( ship→cargo )</text>
  <text x="100" y="244" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">largest first ↓</text>

  <!-- Arrow 1→2 -->
  <line x1="192" y1="140" x2="208" y2="140" stroke="currentColor" stroke-opacity="0.5" stroke-width="1.5" marker-end="url(#arr)"/>

  <!-- Stage 2: find_best_fit_3d -->
  <text x="310" y="36" text-anchor="middle" font-size="12" font-weight="700" fill="currentColor" fill-opacity="0.9">② find_best_fit_3d</text>
  <text x="310" y="52" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.6">bins × spaces × 6 orientations</text>

  <!-- Bins -->
  <rect x="222" y="64" width="176" height="42" rx="4" fill="#12A594" fill-opacity="0.15" stroke="#12A594" stroke-opacity="0.6" stroke-width="1.2"/>
  <text x="310" y="81" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">ForwardHold</text>
  <text x="310" y="97" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.6">z = −8 m  ·  30% weight</text>

  <rect x="222" y="114" width="176" height="42" rx="4" fill="currentColor" fill-opacity="0.09" stroke="currentColor" stroke-opacity="0.3" stroke-width="1.2"/>
  <text x="310" y="131" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.85">AftHold</text>
  <text x="310" y="147" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.6">z = −8 m  ·  30% weight</text>

  <rect x="222" y="164" width="176" height="42" rx="4" fill="currentColor" fill-opacity="0.06" stroke="currentColor" stroke-opacity="0.2" stroke-width="1.2"/>
  <text x="310" y="181" text-anchor="middle" font-size="11" fill="currentColor" fill-opacity="0.7">Deck</text>
  <text x="310" y="197" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">z = 0 m  ·  40% weight</text>

  <text x="310" y="232" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">pick tightest fit</text>
  <text x="310" y="248" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">(min wasted volume)</text>

  <!-- Arrow 2→3 -->
  <line x1="422" y1="140" x2="428" y2="140" stroke="currentColor" stroke-opacity="0.5" stroke-width="1.5" marker-end="url(#arr)"/>

  <!-- Stage 3: Result -->
  <text x="520" y="36" text-anchor="middle" font-size="12" font-weight="700" fill="currentColor" fill-opacity="0.9">③ Result</text>
  <text x="520" y="52" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.6">place_cargo_3d</text>

  <!-- Placed -->
  <rect x="442" y="64" width="156" height="36" rx="4" fill="#12A594" fill-opacity="0.15" stroke="#12A594" stroke-opacity="0.6" stroke-width="1.2"/>
  <text x="520" y="80" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">Placed ✓</text>
  <text x="520" y="95" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.6">pos_x / pos_y / pos_z set</text>

  <rect x="442" y="108" width="156" height="36" rx="4" fill="currentColor" fill-opacity="0.08" stroke="currentColor" stroke-opacity="0.25" stroke-width="1.2"/>
  <text x="520" y="124" text-anchor="middle" font-size="10" fill="currentColor" fill-opacity="0.7">split_space_3d</text>
  <text x="520" y="139" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.55">guillotine cut → new spaces</text>

  <!-- Unplaced sentinel -->
  <rect x="442" y="154" width="156" height="36" rx="4" fill="#D05663" fill-opacity="0.12" stroke="#D05663" stroke-opacity="0.5" stroke-width="1.2"/>
  <text x="520" y="170" text-anchor="middle" font-size="11" font-weight="600" fill="#D05663">Unplaced ⚠</text>
  <text x="520" y="185" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.6">pos_x = −1.0f sentinel</text>

  <text x="520" y="220" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">excluded from</text>
  <text x="520" y="234" text-anchor="middle" font-size="9" fill="currentColor" fill-opacity="0.5">KG / GM calc</text>

  <!-- Arrow marker -->
  <defs>
    <marker id="arr" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L7,3 z" fill="currentColor" fill-opacity="0.45"/>
    </marker>
  </defs>
</svg>

## The Problem: Bin Packing

Imagine a ship as a set of boxes — holds and deck — and the cargo as a pile of smaller boxes that must fit inside. You want to use as much of the available space as possible while respecting weight limits and safety constraints. This is the **bin-packing problem**, and it is computationally hard: finding the provably optimal arrangement takes exponential time for large manifests. In practice, cargo planners (and software) use heuristics — rules that produce good solutions quickly without guaranteeing perfection.

FFD is one of the oldest and best-studied heuristics. It works in two steps:

1. **Sort** items from largest to smallest (Decreasing).
2. **Place each item** into the first bin that can fit it (First Fit).

The intuition: large items are the hard constraints. If you place them first, you learn immediately whether they fit at all, and the smaller items can fill the gaps left behind. If you tried small items first, you might fill a hold with dozens of light boxes and then discover there is no room left for the one 50-tonne machine.

---

## Step 1 — Sort by Volume

CargoForge-C sorts the entire cargo array before touching the bins. The entry point is `place_cargo_3d` in [`src/placement_3d.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/placement_3d.c):

```c
void place_cargo_3d(Ship *ship) {
    // Sort cargo by volume (largest first - FFD heuristic)
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_volume_desc);
    ...
}
```

`qsort` is the C standard-library sort — it takes a pointer to the array, the number of elements, the size of each element, and a comparator function. The comparator is `cargo_cmp_by_volume_desc`:

```c
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

`dimensions[0]`, `dimensions[1]`, `dimensions[2]` are the length, width, and height in metres (the `Cargo` struct from `cargoforge.h`). Volume is their product in cubic metres. The comparator returns `1` when A is *smaller* than B, which tells `qsort` to move B before A — producing descending order.

!!! note "Sort modifies the array"
    After `qsort`, `ship->cargo` is reordered in memory. Cargo IDs and all other fields travel with their elements, so nothing is lost — but any index you cached before the sort is now wrong. CargoForge-C never caches indices across this call.

### Why volume rather than weight?

Volume determines whether an item *fits* geometrically. A 5-tonne machine that is 10 m × 4 m × 3 m may dominate hold layout even though a denser 20-tonne cube is heavier. Sorting by volume gives the packer the best chance of finding room for the physically largest items. Weight constraints are enforced separately during placement by comparing `bin->current_weight + cargo->weight` against `bin->max_weight`.

---

## Step 2 — Three Bins

After sorting, `place_cargo_3d` creates three hard-coded bins that model a typical general-cargo ship's stowage zones:

| Bin | X origin | Width | Max weight |
|---|---|---|---|
| `ForwardHold` | 0 m | `L × 0.30` | `max_weight × 0.30` |
| `AftHold` | `L × 0.70` | `L × 0.30` | `max_weight × 0.30` |
| `Deck` | 0 m | `L` | `max_weight × 0.40` |

The holds are centred at the bow and stern quarters respectively, sit 8 m below the waterline (`z = -8.0f`), and are 80% of the ship's width (`width × 0.8`) to leave room for side tanks. The deck spans the full length at waterline height (`z = 0.0f`) with a 4 m stacking limit and takes up to 40% of total weight capacity.

Each bin starts with a single free `Space3D` that fills its entire volume. As cargo is placed, `split_space_3d` subdivides that space by guillotine cuts — more on that in the next lesson.

---

## Step 3 — Place Each Item: `find_best_fit_3d`

For every cargo item (in sorted order), the packer calls `find_best_fit_3d`:

```c
int find_best_fit_3d(const Ship *ship, Bin3D *bins, int bin_count,
                     const Cargo *cargo, int *best_bin, int *best_space,
                     int *best_orientation) {
    *best_bin = -1;
    *best_space = -1;
    *best_orientation = -1;
    float best_fit_volume = 1e9f;  // Find tightest fit (minimize wasted space)

    for (int b = 0; b < bin_count; b++) {
        Bin3D *bin = &bins[b];

        // Check weight constraint
        if (bin->current_weight + cargo->weight > bin->max_weight) {
            continue;
        }

        for (int s = 0; s < bin->space_count; s++) {
            Space3D *space = &bin->spaces[s];
            if (!space->is_free) continue;

            // Check cargo-specific constraints
            if (ship && !check_cargo_constraints(ship, cargo, bin, space)) {
                continue;  // Constraint violation, skip this placement
            }

            // Try all 6 orientations
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

    return (*best_bin != -1);
}
```

The triple-nested loop visits every bin, every free space within it, and all six axis-aligned orientations of the cargo (all permutations of length/width/height). For each combination that passes the geometric check (`fits_in_space`) and the safety checks (`check_cargo_constraints`), it records the space whose volume is smallest — the *tightest fit*. This variant is sometimes called **Best-Fit Decreasing** rather than pure FFD, and it reduces wasted space compared to stopping at the first space that works.

`get_orientation_dims` produces the six orientations by reassigning which physical dimension maps to width, depth, and height:

```c
static void get_orientation_dims(const Cargo *c, int orientation,
                                 float *w, float *d, float *h) {
    float dims[3] = {c->dimensions[0], c->dimensions[1], c->dimensions[2]};
    switch (orientation) {
        case 0: *w = dims[0]; *d = dims[1]; *h = dims[2]; break; // XYZ
        case 1: *w = dims[0]; *d = dims[2]; *h = dims[1]; break; // XZY
        case 2: *w = dims[1]; *d = dims[0]; *h = dims[2]; break; // YXZ
        case 3: *w = dims[1]; *d = dims[2]; *h = dims[0]; break; // YZX
        case 4: *w = dims[2]; *d = dims[0]; *h = dims[1]; break; // ZXY
        case 5: *w = dims[2]; *d = dims[1]; *h = dims[0]; break; // ZYX
    }
}
```

!!! warning "Orientation 2 vs orientation 4"
    All six permutations of three values are distinct, but several may produce geometrically identical fits when two dimensions are equal (e.g., a cubic container). The code does not deduplicate — it simply picks whichever produces the smallest space volume. In the worst case it examines the same geometry twice, but it never places incorrectly.

---

## Why Sort Order Matters for Stability

Packing efficiency is only half the story. The sort order has a direct effect on ship stability because of *how* the bins are tried in order: `ForwardHold` → `AftHold` → `Deck`.

Large, heavy cargo tends to land in the hold rather than on deck, because:

- The holds are tried first and fill up with large items that cannot fit on the height-limited deck (4 m vs 8 m).
- Heavy items placed low in the hold reduce the ship's **KG** (vertical centre of gravity), which increases **GM** and therefore stability.

If the sort were reversed — small items first — large heavy items would be attempted last, after the holds were already fragmented. Many would spill onto deck, raising KG and potentially causing GM to drop below the IMO minimum of 0.15 m.

The stability chain is:

$$KG = \frac{\text{lightship moment} + \sum w_i \cdot \left(z_i + \tfrac{h_i}{2}\right)}{\text{displacement}}$$

$$GM = KB + BM - KG$$

A lower KG from low-placed heavy items makes $GM$ larger and the GZ curve broader. FFD's sort order is not just a computational convenience — it is a stability policy embedded in the algorithm.

!!! tip "What if an item cannot be placed?"
    When `find_best_fit_3d` returns 0 (no viable position found), `place_cargo_3d` sets `pos_x = pos_y = pos_z = -1.0f` and prints a warning to stderr. The analysis code treats any item with `pos_x < 0` as unplaced and excludes it from all moment calculations. An unplaced item is not lost — it is just not part of the stability picture, and the operator must resolve it manually.

---

## The Full Loop in `place_cargo_3d`

Putting it together:

```c
    int placed_count = 0;
    for (int i = 0; i < ship->cargo_count; i++) {
        Cargo *c = &ship->cargo[i];
        int best_bin, best_space, best_orientation;

        if (find_best_fit_3d(ship, bins, bin_count, c,
                             &best_bin, &best_space, &best_orientation)) {
            Bin3D *bin = &bins[best_bin];
            Space3D *space = &bin->spaces[best_space];

            c->pos_x = space->x;
            c->pos_y = space->y;
            c->pos_z = space->z;

            bin->current_weight += c->weight;
            split_space_3d(bin, best_space, c, best_orientation);
            placed_count++;
        } else {
            fprintf(stderr, "Warning: Could not place cargo %s ...\n", c->id);
            c->pos_x = c->pos_y = c->pos_z = -1.0f;
        }
    }
```

After every successful placement, `split_space_3d` subdivides the occupied space into up to three new free rectangles (the right, back, and top remainders), appending them to `bin->spaces[]`. The next item will see these new, smaller spaces as candidates. This guillotine cut is the subject of the next lesson.

---

## Recap

- **FFD sorts cargo by volume descending** before any placement decision is made, so large items are never squeezed out by fragmentation caused by earlier small items.
- `cargo_cmp_by_volume_desc` multiplies the three `dimensions[]` fields and returns a descending ordering for `qsort`.
- `find_best_fit_3d` iterates bins → free spaces → six orientations, keeping the candidate that wastes the least space — a Best-Fit variant of FFD.
- The bin order (holds before deck) combined with the descending sort naturally places heavy cargo low, reducing KG and improving GM.
- Items that cannot be placed receive `pos_x = -1.0f` as a sentinel and are excluded from stability analysis.
- The placement loop calls `split_space_3d` after each success, subdividing the used space into up to three new free rectangles for future items.

*Next: [3D bins and weight limits](32-3d-bins-and-weight.md).*
