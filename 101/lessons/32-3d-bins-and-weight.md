# 3D bins and weight limits

Before CargoForge-C can compute a ship's centre of gravity or check IMO stability
criteria, every cargo item needs a position in 3D space. `place_cargo_3d` in
`src/placement_3d.c` does that job: it divides the ship into rectangular zones called
**bins**, then assigns each piece of cargo to a position inside one of them while
tracking how much weight each bin is already carrying.

---

## What a bin is

A bin is a named, axis-aligned rectangular box that represents a usable volume on the
ship. In CargoForge-C the `Bin3D` struct (declared in `placement_3d.h`) carries
everything the placer needs to know about that zone:

| Field | Meaning |
|---|---|
| `name` | Human-readable label: `"ForwardHold"`, `"AftHold"`, `"Deck"` |
| `x`, `y`, `z` | Position of the bin's origin (metres from the hull origin) |
| `width`, `depth`, `height` | Extent along X, Y, Z (metres) |
| `max_weight` | Hard weight ceiling for this bin (kg) |
| `current_weight` | Running total of placed cargo weight (kg) |
| `spaces[]` | Array of `Space3D` free rectangles available for new cargo |
| `space_count` | How many entries in `spaces[]` are valid |

A `Space3D` is a sub-region inside a bin that is still available. Every bin starts with
one space that fills it completely. As cargo is placed, that space is carved up into
smaller free pieces — the guillotine algorithm described in Lesson 33.

---

## The three bins CargoForge-C creates

`place_cargo_3d` hard-codes three bins from the ship's own dimensions. From
`src/placement_3d.c` (lines 163–218):

```c
// Forward hold (30% of length)
bins[0] = (Bin3D){
    .name = "ForwardHold",
    .x = 0.0f, .y = 0.0f, .z = -8.0f,   // Below waterline
    .width  = ship->length * 0.3f,
    .depth  = ship->width  * 0.8f,        // Leave space for side tanks
    .height = 8.0f,                        // Typical hold height
    .max_weight   = ship->max_weight * 0.3f,
    .current_weight = 0.0f,
    .space_count = 1
};

// Aft hold (30% of length)
bins[1] = (Bin3D){
    .name = "AftHold",
    .x = ship->length * 0.7f, .y = 0.0f, .z = -8.0f,
    .width  = ship->length * 0.3f,
    .depth  = ship->width  * 0.8f,
    .height = 8.0f,
    .max_weight   = ship->max_weight * 0.3f,
    .current_weight = 0.0f,
    .space_count = 1
};

// Deck (full length, lower weight capacity)
bins[2] = (Bin3D){
    .name = "Deck",
    .x = 0.0f, .y = 0.0f, .z = 0.0f,    // At waterline
    .width  = ship->length,
    .depth  = ship->width,
    .height = 4.0f,                        // Lower stacking on deck
    .max_weight   = ship->max_weight * 0.4f,
    .current_weight = 0.0f,
    .space_count = 1
};
```

Three design choices are worth understanding:

### Proportional sizing

Each bin's dimensions are a fixed fraction of `ship->length` and `ship->width`.
If you load a 200 m × 30 m ship, the ForwardHold will be 60 m × 24 m. The two
holds each hold **30 %** of `max_weight`; the Deck holds **40 %**. Together that is
exactly 100 % — so if every bin filled to its limit, the ship would reach its
maximum permitted cargo weight.

### The z-coordinate and what it means

The ship's coordinate origin sits at the hull — the lowest point of the keel is
near z = −8 m (using the box-hull's 8 m hold height). `pos_z` on a placed cargo
item therefore represents height above (or depth below) the keel datum:

| Bin | `z` | Interpretation |
|---|---|---|
| ForwardHold | −8.0 m | Holds are below waterline; z rises toward 0 as cargo stacks up |
| AftHold | −8.0 m | Same datum |
| Deck | 0.0 m | Deck cargo sits at (or above) the waterline |

This matters directly for stability: the analysis module computes KG (vertical
centre of gravity) as:

$$KG = \frac{\sum w_i \cdot \left(z_i + \frac{h_i}{2}\right) + \text{lightship moment}}{\text{displacement}}$$

Cargo deep in the holds (large negative z) pulls KG down; deck cargo (z = 0 or
positive) pushes KG up. Lower KG → larger GM → safer ship. The bin geometry
therefore shapes the stability outcome directly.

### Width = 80 % for the holds

Both holds use `ship->width * 0.8` rather than the full beam. The remaining 20 %
is notionally reserved for side tanks and structural frames — a simplification that
keeps the geometry from assigning cargo to unreachable corners.

---

## The placement loop

After the bins are built, `place_cargo_3d` processes every cargo item in the order
determined by the FFD sort (largest volume first — covered in Lesson 31). From
`src/placement_3d.c` (lines 220–256):

```c
for (int i = 0; i < ship->cargo_count; i++) {
    Cargo *c = &ship->cargo[i];
    int best_bin, best_space, best_orientation;

    if (find_best_fit_3d(ship, bins, bin_count, c,
                         &best_bin, &best_space, &best_orientation)) {
        Bin3D  *bin   = &bins[best_bin];
        Space3D *space = &bin->spaces[best_space];

        // Set cargo position
        c->pos_x = space->x;
        c->pos_y = space->y;
        c->pos_z = space->z;

        // Update bin weight
        bin->current_weight += c->weight;

        // Split the space
        split_space_3d(bin, best_space, c, best_orientation);

        placed_count++;
    } else {
        fprintf(stderr, "Warning: Could not place cargo %s ...\n", c->id);
        // Mark as unplaced
        c->pos_x = c->pos_y = c->pos_z = -1.0f;
    }
}
```

Two outcomes are possible for every item:

- **Placed** — `find_best_fit_3d` found a viable space. The item's `pos_x/y/z`
  are set to the corner of that space, the bin's `current_weight` is incremented,
  and the space is split into free remainders.
- **Unplaced** — no space passed all the checks. The sentinel value
  `pos_x = pos_y = pos_z = -1.0f` is written. Every downstream calculation that
  sums cargo moments checks `pos_x >= 0` before including an item, so unplaced
  cargo is silently excluded from the stability analysis.

!!! warning "Silent exclusion from analysis"
    If a heavy item cannot be placed, it disappears from the stability calculation.
    The printed warning on stderr is the only signal. Always check the placement
    summary lines for `n/N items placed` before trusting an analysis result.

---

## How `find_best_fit_3d` enforces weight limits

Weight is the very first check inside `find_best_fit_3d`. From
`src/placement_3d.c` (lines 61–67):

```c
for (int b = 0; b < bin_count; b++) {
    Bin3D *bin = &bins[b];

    // Check weight constraint
    if (bin->current_weight + cargo->weight > bin->max_weight) {
        continue;
    }
    /* ... rest of space/orientation search ... */
}
```

If adding this item would push the bin past its `max_weight`, the entire bin is
skipped without even looking at individual spaces. This is O(1) per bin and
happens before the more expensive constraint checks.

Only after passing the weight gate does the code walk every free space and try all
six axis-aligned orientations. The chosen space is the one with the smallest
volume that still fits the item — a **best-fit** strategy that leaves the largest
spaces available for future items:

```c
float best_fit_volume = 1e9f;   // Start with "impossibly large"

for (int o = 0; o < 6; o++) {
    if (fits_in_space(space, cargo, o)) {
        float vol = space_volume(space);
        if (vol < best_fit_volume) {   // Prefer smaller spaces
            best_fit_volume = vol;
            *best_bin = b;
            *best_space = s;
            *best_orientation = o;
        }
    }
}
```

`fits_in_space` is a straightforward dimension check — width, depth, and height of
the cargo (in the chosen orientation) must each be ≤ the corresponding space
dimension. `space_volume` is simply `width × depth × height`.

### When weight forces a different bin

Suppose ForwardHold has 595 t of its 600 t capacity used and a 10 t item arrives.
The weight check fails for ForwardHold. The code moves on to AftHold, then Deck.
If neither can take the item either, `find_best_fit_3d` returns 0 (failure) and
the item is marked unplaced.

---

## Six orientations — why they matter

A container that is 6 m × 2.5 m × 2.6 m can be loaded in any of six axis-aligned
rotations. From `src/placement_3d.c`:

```c
switch (orientation) {
    case 0: *w = dims[0]; *d = dims[1]; *h = dims[2]; break; // XYZ (as-declared)
    case 1: *w = dims[0]; *d = dims[2]; *h = dims[1]; break; // XZY
    case 2: *w = dims[1]; *d = dims[0]; *h = dims[2]; break; // YXZ
    case 3: *w = dims[1]; *d = dims[2]; *h = dims[0]; break; // YZX
    case 4: *w = dims[2]; *d = dims[0]; *h = dims[1]; break; // ZXY
    case 5: *w = dims[2]; *d = dims[1]; *h = dims[0]; break; // ZYX
}
```

All six permutations of the three dimensions are tried for every candidate space.
This means a tall, narrow crate that won't stand upright in a short space might
still fit if rotated onto its side. The best-fitting orientation is kept.

!!! note "Real-world constraint not yet modelled"
    CargoForge-C does not yet enforce "this-way-up" markings. Every cargo item is
    treated as freely rotatable. Fragile or liquid cargo in practice has orientation
    constraints that a future version could add to the `Cargo` struct.

---

## Placement summary output

After the loop, `place_cargo_3d` prints a summary to stderr. Each bin reports its
used weight versus its limit:

```
3D Placement complete: 8/10 items placed
  ForwardHold: 580000.0 / 600000.0 kg (96.7% capacity)
  AftHold:     420000.0 / 600000.0 kg (70.0% capacity)
  Deck:        200000.0 / 800000.0 kg (25.0% capacity)
```

This is the first place to look when an item fails to place — a bin at 96 % with a
large remaining item is a weight-limit rejection, not a geometry failure.

---

## How position feeds into analysis

Once `place_cargo_3d` returns, every item that was placed has valid `pos_x`,
`pos_y`, `pos_z` coordinates. `perform_analysis` in `analysis.c` then does a
single pass over `ship->cargo`, skipping items where `pos_x < 0`:

- `pos_z + dimensions[2]/2` is the vertical centroid of the item — used to
  compute the cargo's contribution to KG.
- `pos_x` relative to `L/2` determines the item's longitudinal moment — used for
  trim and for the shear-force/bending-moment distribution.
- `pos_y` relative to `B/2` determines the transverse moment — used for heel.

The bin structure itself is discarded after `place_cargo_3d` returns; only the
coordinates written onto each `Cargo` struct persist into analysis.

---

## Recap

- CargoForge-C partitions the ship into three hard-coded `Bin3D` zones:
  **ForwardHold** (30 % length, z = −8 m), **AftHold** (30 % length, z = −8 m),
  and **Deck** (full length, z = 0 m).
- Each bin carries a weight ceiling (`max_weight`): 30 % / 30 % / 40 % of the
  ship's `max_weight` respectively.
- `find_best_fit_3d` rejects a bin immediately if adding the item would exceed
  `bin->current_weight + cargo->weight > bin->max_weight`.
- Items that pass the weight gate are tried in all **six axis-aligned orientations**;
  the tightest-fitting space (smallest space volume that still accommodates the item)
  wins.
- Items that cannot be placed receive the sentinel `pos_x = pos_y = pos_z = -1.0f`
  and are excluded from every downstream stability calculation.
- The `pos_x/y/z` coordinates written by the placer feed directly into KG, trim,
  heel, and longitudinal-strength calculations in `analysis.c`.

*Next: [Constraints: segregation and stackability](33-constraints-segregation-stackability.md).*
