# Lab 8 - Place cargo and trigger a violation

In this lab you run CargoForge-C's 3D bin-packing engine against real input files, read the
position coordinates it assigns, then deliberately construct two dangerous-goods items that are
chemically incompatible and watch the IMDG segregation engine reject their placement. By the end
you will be able to read the `Pos (x, y, z)` output, understand what each coordinate means,
and connect live constraint messages to the code that produces them.

---

## Prerequisites

You need a working binary. From the repository root:

```bash
make
```

The binary lands at `./cargoforge`. All commands below are run from the repository root.

---

## Step 1 — Run `optimize` on the sample manifest

The `examples/` directory ships two ready-made input files: a ship config and a plain cargo
manifest (no dangerous goods yet).

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo.txt
```

**Expected output (key lines):**

```
[OK] Ship configuration loaded
[OK] Cargo manifest loaded

Running 3D bin-packing...
3D Placement complete: 5/5 items placed
  ForwardHold: 1500000.0 / 15000001.0 kg (10.0% capacity)
  AftHold: 0.0 / 15000001.0 kg (0.0% capacity)
  Deck: 0.0 / 20000000.0 kg (0.0% capacity)
[OK] Optimization complete
```

Then the stability report prints each item with its assigned position:

```
  - HeavyMachinery  | Pos (   0.00,    0.00,  -8.00) | 550.00 t
  - ContainerA      | Pos (   0.00,    0.00,  -5.00) | 250.00 t
  - ContainerB      | Pos (   0.00,    2.40,  -5.00) | 250.00 t
  - SteelBeams      | Pos (   0.00,    5.00,  -8.00) | 400.00 t
  - SmallCrate      | Pos (   0.00,    0.00,  -2.40) |  50.00 t
```

### Reading the coordinates

Each `Pos (x, y, z)` is in metres from the hull's keel origin:

| Axis | Direction | Reference |
|------|-----------|-----------|
| `x`  | Fore–aft (positive = bow direction) | 0 = stern end of the ship |
| `y`  | Port–starboard (positive = starboard) | 0 = port side |
| `z`  | Vertical (positive = upward) | 0 = keel |

All three values here are **the item's lower-forward-port corner**, not its centre. A negative
`z` means the item sits below the keel datum — this is normal for hold cargo, because the
holds are modelled at `z = -8 m` (the hold floor) relative to the keel reference plane used
internally.

!!! note "Why z = -8 m?"
    The three hard-coded bins in `placement_3d.c` place the forward and aft holds at
    `z = -8 m` with a height of 8 m, so the top of a hold item reaches z = 0 (the waterline
    datum). The deck bin sits at `z = 0` with a height of 4 m.

---

## Step 2 — Inspect the ASCII layout

Below the stability numbers, `optimize` prints a top-down ASCII map. The ship is 150 m × 25 m;
`#` marks occupied cells.

```
   +--------------------------------------------------------------------------------+
 0 |###########.....................................................................|
 1 |###########.....................................................................|
 2 |###########.....................................................................|
 3 |###########.....................................................................|
```

All five items landed in the forward portion of the ship because `place_cargo_3d` fills the
ForwardHold first (First Fit Decreasing order, largest volume placed first). The AftHold and
Deck bins remain empty.

---

## Step 3 — Run `info` to see the cargo summary

```bash
./cargoforge info examples/sample_ship.cfg examples/sample_cargo.txt
```

This prints aggregate statistics without running the full optimizer, useful for a quick sanity
check before committing to a placement run.

---

## Step 4 — Validate input without placing

```bash
./cargoforge validate examples/sample_ship.cfg examples/sample_cargo.txt
```

**Expected output:**

```
Validating ship configuration: examples/sample_ship.cfg
[OK] Ship configuration is valid

Validating cargo manifest: examples/sample_cargo.txt
[OK] Cargo manifest is valid

[OK] All validation checks passed!
```

`validate` parses both files and reports errors without running bin-packing or stability
analysis. Use it when you have edited an input file and want a fast format check.

---

## Step 5 — Run `optimize` on the DG manifest

[`examples/sample_cargo_dg.txt`](https://github.com/alikatgh/CargoForge-C/blob/main/examples/sample_cargo_dg.txt) adds three dangerous-goods items to the same ship. Look at the
file:

```
# ID              Weight(t) Dimensions(LxWxH)   Type        DG:class.div:UNnumber:stowage:EmS
HeavyMachinery    550       20x5x3              standard
FlammableLiquid   25        6x2.5x2.6           hazardous   DG:3.1:UN1203:A:F-E,S-D
OxidizerDrum      15        6x2.5x2.6           hazardous   DG:5.1:UN1942:A:F-A,S-Q
CorrosiveAcid     20        6x2.5x2.6           hazardous   DG:8.0:UN1789:A:F-A,S-B
ContainerA        250       12.2x2.4x2.6        reefer
SmallCrate        50        2x2x2               standard
```

The DG field format is: `DG:<class>[.<division>]:<UNnumber>:<stowage>:<EmS>`.

Now run:

```bash
./cargoforge optimize examples/sample_ship.cfg examples/sample_cargo_dg.txt
```

**Expected output (constraint lines):**

```
Constraint: OxidizerDrum too close to FlammableLiquid (IMDG Separated from: 2.6m < 6.0m)
Constraint: CorrosiveAcid too close to FlammableLiquid (IMDG Away from: 2.4m < 3.0m)
Constraint: CorrosiveAcid too close to OxidizerDrum (IMDG Separated from: 2.5m < 6.0m)
...
3D Placement complete: 6/6 items placed
```

The constraint messages appear during the bin-packing search, each time the engine tries a
candidate space and finds the IMDG distance rule would be violated. The engine keeps trying
orientations and positions until it finds one that satisfies all constraints — or, if none
exists, places the item anyway at the best available slot (all items still end up placed in
this example because the ship is large relative to the cargo).

### What the distances mean

| Segregation type | Minimum horizontal distance |
|------------------|-----------------------------|
| Away from        | 3 m                         |
| Separated from   | 6 m                         |
| Separated by complete compartment | 12 m      |
| Separated longitudinally | 24 m              |
| Incompatible (hard reject) | — (reject entirely) |

The IMDG Code Table 7.2.4 matrix (`seg_matrix` in `imdg.c`) determines which category applies
to each class-pair. Class 3.1 (flammable liquid) vs. class 5.1 (oxidizer) is "Separated from",
hence the 6 m minimum. Class 3.1 vs. class 8.0 (corrosive) is "Away from", hence 3 m.

---

## Step 6 — Construct a manifest with an incompatible pair

Some class pairs are flagged `SEG_INCOMPATIBLE` in the matrix — the engine rejects any
candidate space for the second item if the first is already placed. Create this file at
`/tmp/incompatible.txt`:

```
# Two incompatible DG items — Class 1.1 explosive vs Class 5.1 oxidizer
ExplosiveCharge   10   4x2x2   hazardous   DG:1.1:UN0083:D:F-B,S-X
OxidizerDrum      15   4x2x2   hazardous   DG:5.1:UN1942:A:F-A,S-Q
```

!!! note "Stowage categories"
    `D` = on deck only. `A` = any location (under deck or on deck).

Now run:

```bash
./cargoforge optimize examples/sample_ship.cfg /tmp/incompatible.txt
```

Watch the constraint messages. Because the ship has three large bins, the engine may still find
separated positions for both items — "incompatible" triggers a hard reject only when the
horizontal distance to an already-placed incompatible item is zero (i.e., the candidate
overlaps). For a truly forced collision, reduce the ship size so the bins are tiny, or add
enough other cargo to fill the hold.

The reliable way to see a final IMDG report is via the JSON output, which includes the IMDG
check section:

```bash
./cargoforge optimize examples/sample_ship.cfg /tmp/incompatible.txt --format=json 2>/dev/null
```

Look for the `"imdg"` key in the JSON. If violations remain after placement, they appear in the
`"violations"` array with each pair's class combination, the required minimum distance, and the
actual edge-to-edge distance measured.

---

## Step 7 — Build and run the test suite

The unit tests exercise all the modules touched by this lab:

```bash
make test
```

To run with AddressSanitizer and Undefined Behaviour Sanitizer:

```bash
make test-asan
```

Each test binary name maps to one module:

| Binary | What it covers |
|--------|---------------|
| `test_parser` | Config and manifest parsing, including DG field grammar |
| `test_constraints` | Point-load, IMDG distance, stacking-pressure checks |
| `test_imdg` | Segregation matrix lookups and distance thresholds |

---

## Solution

### Step 5 — why all items still place despite constraint warnings

The constraint messages are printed every time a candidate space is **rejected**. They do not
mean the item was ultimately unplaced — they mean that particular (bin, space, orientation)
triple failed, and the engine moved on to the next candidate. All 6 items in the DG manifest
are placed because the 150 m ship has enough room to satisfy minimum distances even though
individual spaces in the forward hold are rejected.

An item gets `pos_x = pos_y = pos_z = -1.0` only when **every** candidate across all bins,
spaces, and orientations fails. The optimize output line `3D Placement complete: N/N items
placed` confirms whether any items were left unplaced.

### Step 6 — forcing a genuine incompatible rejection

To guarantee that `OxidizerDrum` cannot be placed, fill the ship with other cargo until only
one space remains, and place `ExplosiveCharge` there first (it will be placed first because
bin-packing sorts by volume descending). With no remaining space outside `SEG_INCOMPATIBLE`
distance, the output will show `1/2 items placed` and the aft item will have `pos_x = -1.0`.

### Reading `pos_x = -1.0` in JSON output

In JSON mode (`--format=json`), unplaced items appear as:

```json
{
  "id": "OxidizerDrum",
  "placed": false,
  "position": {"x": -1.0, "y": -1.0, "z": -1.0}
}
```

The `-1.0` sentinel is set by the parser's initialisation code and left unchanged when
`find_best_fit_3d` returns 0 (no viable space found).

---

## Recap

- `./cargoforge optimize <ship> <cargo>` runs the full pipeline: parse → 3D bin-pack →
  stability analysis → ASCII layout.
- Each `Pos (x, y, z)` is the lower-forward-port corner of the item, in metres from the hull
  origin; holds sit at negative z.
- The DG field grammar is `DG:<class>.<div>:<UN>:<stowage>:<EmS>`; `parse_dg_field` in
  `parser.c` validates class range 1–9 and returns `NULL` for malformed fields.
- Constraint messages during bin-packing indicate rejected candidate spaces, not unplaced
  items; check the `N/N items placed` summary line.
- IMDG segregation minimum distances are: Away from = 3 m, Separated from = 6 m, Separated
  by complete compartment = 12 m, Separated longitudinally = 24 m.
- `make test` and `make test-asan` verify constraint and IMDG logic without running the full
  optimizer.

*Next: [The analysis module](34-the-analysis-module.md).*
