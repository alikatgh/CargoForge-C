# Structs, enums, and the data model

Every CargoForge-C operation — parsing a manifest, running stability analysis, placing cargo in a hold — ultimately reads or writes two core structs: `Cargo` and `Ship`. Understanding their fields is understanding the program. This lesson walks each field in detail, explains the units, shows how optional extensions hang off the struct as pointers, and introduces the enums that control output behavior.

## The mental model 🧠

A **struct** is a labelled form, and every `Ship` in memory is one filled-out copy of it. Instead of juggling a dozen loose variables — length here, width there, a weight somewhere else — you staple them onto one sheet called `Ship` and pass the whole clipboard around by its address.

One field is special. `cargo` isn't data itself — it's an *arrow* pointing at a separate stack of `Cargo` forms allocated elsewhere (the heap, Lesson 10). That indirection is the trick that lets a fixed-size `Ship` carry a cargo list of any length: the struct stays the same few bytes whether it holds three crates or three thousand; only the thing the arrow points at grows.

An **enum** is the form's drop-down menu. Rather than a free-text "output format" field where someone could scribble anything, the value can only be one of a named, fixed set — so an impossible state can't be written down in the first place. Get the data model right and half the bugs in the rest of the program simply cannot occur.

<svg viewBox="0 0 600 250" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>A Ship struct holds scalar fields plus a pointer to a separate Cargo array</title>
<desc>The Ship struct bundles fixed fields such as length, width, and cargo_count, plus a cargo pointer. That pointer points at a separately allocated array of Cargo structs on the heap, so one fixed-size Ship can carry a cargo list of any length.</desc>
<rect x="24" y="26" width="190" height="200" rx="6" fill="#12A594" fill-opacity="0.05" stroke="#12A594" stroke-width="1.3"/>
<text x="119" y="47" font-size="12.5" text-anchor="middle" fill="#12A594" font-family="var(--md-code-font,monospace)">struct Ship</text>
<line x1="34" y1="56" x2="204" y2="56" stroke="currentColor" stroke-opacity="0.15"/>
<text x="40" y="80" font-size="11" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">float length</text><text x="198" y="80" font-size="11" text-anchor="end" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">100.0</text>
<text x="40" y="104" font-size="11" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">float width</text><text x="198" y="104" font-size="11" text-anchor="end" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">20.0</text>
<text x="40" y="128" font-size="11" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">int cargo_count</text><text x="198" y="128" font-size="11" text-anchor="end" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">3</text>
<rect x="32" y="150" width="174" height="64" rx="4" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-opacity="0.5"/>
<text x="40" y="172" font-size="11" fill="currentColor" opacity="0.85" font-family="var(--md-code-font,monospace)">Cargo *cargo</text>
<text x="40" y="190" font-size="9" fill="currentColor" opacity="0.55">a pointer, not the data —</text>
<text x="40" y="205" font-size="9" fill="currentColor" opacity="0.55">it points to the heap →</text>
<circle cx="196" cy="183" r="4" fill="#12A594"/>
<path d="M196,183 C250,183 270,71 316,71" fill="none" stroke="#12A594" stroke-width="1.2"/>
<path d="M309,67 L317,71 L309,76" fill="none" stroke="#12A594" stroke-width="1.2"/>
<text x="403" y="40" font-size="9.5" text-anchor="middle" fill="currentColor" opacity="0.55">heap — malloc(capacity × sizeof(Cargo))</text>
<rect x="318" y="50" width="170" height="42" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/>
<text x="328" y="68" font-size="10.5" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">Cargo[0]</text>
<text x="328" y="83" font-size="8.5" fill="currentColor" opacity="0.5">id, weight, pos_x/y/z</text>
<rect x="318" y="100" width="170" height="42" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/>
<text x="328" y="118" font-size="10.5" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">Cargo[1]</text>
<text x="328" y="133" font-size="8.5" fill="currentColor" opacity="0.5">id, weight, pos_x/y/z</text>
<rect x="318" y="150" width="170" height="42" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/>
<text x="328" y="168" font-size="10.5" fill="currentColor" opacity="0.8" font-family="var(--md-code-font,monospace)">Cargo[2]</text>
<text x="328" y="183" font-size="8.5" fill="currentColor" opacity="0.5">id, weight, pos_x/y/z</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **`Cargo` struct** = "a single item's shipping file, in memory" — just as a superintendent's clipboard has one row per crate, `Cargo` holds the id, weight (in kg), dimensions, type, and the three `pos_x/y/z` coordinates that say where it ended up in the hold.
- **Placement sentinel (`pos_x = -1.0f`)** = "this item hasn't been put anywhere yet" — `perform_analysis` checks `pos_x >= 0` before adding an item to any centre-of-gravity sum, so a crate that didn't fit in a bin is automatically ignored rather than silently corrupting the numbers.
- **`dg` pointer (NULL or heap)** = "dangerous-goods metadata only when you actually need it" — ordinary cargo pays zero memory cost for DG fields because `dg` is just a `NULL` pointer; it grows into a real heap allocation only when the manifest line carries a `DG:` tag.
- **`Ship`'s optional extension pointers (`hydro`, `tanks`, `strength_limits`)** = "plug in richer physics only when the config supplies it" — when these are NULL the analysis falls back to simple box-hull formulas; when they point to loaded data, `perform_analysis` interpolates from hydrostatic tables, applies free-surface corrections, or checks class-society strength limits — all without changing any calling code.
- **`AnalysisResult`** = "the full stability report card, returned on the stack" — because it holds no heap pointers it can be returned by value from `perform_analysis` and copied freely; `gm_corrected` (GM minus the free-surface correction) is the single number that determines whether the vessel is safe.
- **`OutputFormat` enum** = "a named switch with five positions" — instead of passing a raw integer to `output_results`, the code passes `FORMAT_JSON` or `FORMAT_TABLE`, so the compiler can warn if a future `switch` forgets to handle a new format.
- **`ship_cleanup`** = "the one safe exit door for all heap memory" — it walks the `cargo[]` array freeing each `dg` pointer, then frees `cargo`, then frees the three extension pointers; calling it once is always safe because every `free` is guarded by a NULL check.

**Why it matters:** if you misread the sentinel and include unplaced cargo in the CG sum, the stability numbers are wrong and the vessel could be declared safe when it isn't; if you bypass `ship_cleanup` and free `ship->cargo` directly, every `DGInfo_` allocation leaks.

---

## Why structs mirror the physical world

In C a **struct** is a named block of memory that groups related variables together. Where a ship superintendent keeps a clipboard with columns for item ID, mass, and location, CargoForge-C keeps a `Cargo` struct. The one-to-one correspondence between the data model and the domain is deliberate: when you see `pos_x`, you should picture a tape measure running from the bow along the ship's centreline.

---

## The `Cargo` struct

From [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h#L39) (line 39):

```c
typedef struct {
    char  id[32];
    float weight;
    float dimensions[MAX_DIMENSION];
    char  type[16];
    float pos_x;
    float pos_y;
    float pos_z;
    struct DGInfo_ *dg;
} Cargo;
```

### Field-by-field

| Field | C type | Unit / range | What it means |
|---|---|---|---|
| `id` | `char[32]` | UTF-8 string, max 31 chars + NUL | User-assigned identifier — e.g. `HeavyMachinery`, `FlammableLiquid`. The 32-byte cap avoids unbounded strings. |
| `weight` | `float` | Kilograms | The parser reads tonnes from the manifest and multiplies by 1000 before storing here. So a manifest line of `550` tonnes becomes `550000.0f` kg in memory. |
| `dimensions[3]` | `float[3]` | Metres (Length × Width × Height) | Index 0 = length along ship axis, 1 = width, 2 = height. `MAX_DIMENSION` is defined as 3 — the array never grows, and the constant documents the intent. |
| `type` | `char[16]` | String token | One of `"standard"`, `"bulk"`, `"reefer"`, `"hazardous"`, `"fragile"`, `"general"`. The constraint engine (`constraints.c`) checks this field with string comparisons such as `is_fragile()` and `is_reefer()`. |
| `pos_x` | `float` | Metres from hull origin | Longitudinal position after the optimizer runs. Before placement, this is `-1.0f` — a sentinel meaning "not yet placed". |
| `pos_y` | `float` | Metres from hull origin | Transverse position, same sentinel convention. |
| `pos_z` | `float` | Metres from hull origin (keel) | Vertical position. A negative value means the cargo centre is below the waterline. |
| `dg` | `struct DGInfo_ *` | Pointer (heap-allocated or NULL) | Points to dangerous-goods metadata when the manifest line includes a `DG:` field. `NULL` for ordinary cargo. Keeping DG data in a separate heap allocation means the common case — non-DG cargo — pays no memory cost for DG fields. |

!!! note "The sentinel value"
    `pos_x < 0` is the program-wide test for "this cargo item has not been placed". Analysis code in `analysis.c` checks `cargo[i].pos_x >= 0` before including an item in centre-of-gravity calculations. If you add a new analysis function, follow the same convention.

### The placement sentinel in practice

`perform_analysis` in `analysis.c` sums moments only over placed items:

```c
/* Only sum moments for placed cargo */
if (ship->cargo[i].pos_x >= 0) {
    total_cargo_weight_kg += ship->cargo[i].weight;
    /* ... accumulate moments ... */
}
```

An item that failed to fit in any bin stays at `pos_x = -1.0f` and is silently excluded from the stability numbers. The `placed_item_count` field of `AnalysisResult` records how many items actually made it aboard.

---

## The `Ship` struct

From [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h#L53) (line 53):

```c
typedef struct {
    float length;
    float width;
    float max_weight;
    int   cargo_count;
    int   cargo_capacity;
    Cargo *cargo;
    float lightship_weight;
    float lightship_kg;

    struct HydroTable_     *hydro;
    struct TankConfig_     *tanks;
    struct StrengthLimits_ *strength_limits;
} Ship;
```

### Hull geometry and limits

| Field | Unit | Meaning |
|---|---|---|
| `length` | Metres | Length between perpendiculars (or overall — the config key is `length_m`). Used in draft, trim, BM, and longitudinal-strength calculations. |
| `width` | Metres | Moulded breadth (`width_m` in the config). Drives the transverse second moment of waterplane area. |
| `max_weight` | Kilograms | Deadweight capacity. The parser converts tonnes → kg. If the total displacement would exceed this, the analysis rejects the load before computing stability. |

### The cargo array — dynamic allocation

```c
int   cargo_count;
int   cargo_capacity;
Cargo *cargo;
```

`cargo` is a heap-allocated array — `cargo_count` items are valid, `cargo_capacity` is how many slots were allocated. The parser calls `malloc` (or `realloc`) to grow the array as it reads each manifest line. This two-integer idiom — count versus capacity — is idiomatic C for a resizable array without depending on C++ vectors or any library.

!!! warning "Ownership"
    The `cargo` array is owned by `Ship`. The only correct way to release it is through `ship_cleanup`, which also walks the array freeing each `cargo[i].dg` pointer before freeing `cargo` itself. Freeing `ship->cargo` directly would leak every `DGInfo_` allocation.

### Lightship fields

| Field | Unit | Meaning |
|---|---|---|
| `lightship_weight` | Kilograms | Mass of the vessel itself — hull, machinery, crew, stores — before any cargo. The parser converts from the `lightship_weight_tonnes` key. |
| `lightship_kg` | Metres | Vertical centre of gravity of the lightship alone, above the keel. Combined with cargo vertical moments, this gives the loaded KG. |

The distinction matters because the lightship KG is fixed by the ship's design and was measured at an inclining experiment. Cargo KGs are computed from the placement positions `pos_z[i] + dimensions[i][2] / 2` (centre of the item).

### Optional extension pointers — the NULL-means-disabled pattern

```c
struct HydroTable_     *hydro;
struct TankConfig_     *tanks;
struct StrengthLimits_ *strength_limits;
```

All three are NULL when the corresponding config key is absent. The analysis engine degrades gracefully:

| Pointer | NULL behaviour | Non-NULL behaviour |
|---|---|---|
| `hydro` | Box-hull formulas (draft from L×B×0.75, KB = 0.53 × draft, BM from rectangular waterplane) | Interpolates KB, BM, draft from the CSV hydrostatic table — more accurate for real hull forms |
| `tanks` | No free-surface correction; no ballast weight | Computes FSM per tank (ρ × l × b³ / 12), virtual KG rise, and adds tank weight to displacement |
| `strength_limits` | `strength_compliant = -1` (not checked) | Compares computed SWSF and SWBM against class-society limits; sets `strength_compliant` to 1 or 0 |

This pattern — "pointer, NULL-disabled, non-NULL-enabled" — lets the same `Ship` struct serve a simple box-hull demo and a fully instrumented vessel without conditional compilation or separate struct types.

---

## The `AnalysisResult` and `CG` structs

`AnalysisResult` (line 81) is what `perform_analysis` returns. It is a plain value type — no heap allocations, no pointers — which means it is safe to return by value on the stack and copy freely.

```c
typedef struct {
    float perc_x;   /* longitudinal CG as % of ship length */
    float perc_y;   /* transverse CG as % of ship width */
} CG;
```

`perc_x = 50` means the cargo CG sits exactly at midship; `perc_y = 50` means it is on the centreline. These percentages are easier for an operator to judge at a glance than raw coordinates.

### Key `AnalysisResult` fields

| Field | Unit | Interpretation |
|---|---|---|
| `draft` | m | Mean draught at displacement. Compare against the ship's marks. |
| `kb` | m | Centre of buoyancy height above keel — higher as draught increases. |
| `bm` | m | Metacentric radius: $BM = I_T / \nabla$ where $I_T$ is the second moment of the waterplane area and $\nabla$ is the displaced volume. |
| `kg` | m | Loaded vertical CG above keel: $KG = \frac{\sum m_i \cdot z_i}{\sum m_i}$. |
| `gm` | m | $GM = KB + BM - KG$. Positive = stable; the higher, the stiffer. |
| `free_surface_correction` | m | Virtual rise in KG caused by liquid sloshing in partially-filled tanks. |
| `gm_corrected` | m | $GM_{corrected} = GM - FSC$. This is the value used for all stability criteria. |
| `trim` | m | Positive = stern deeper than bow. |
| `heel` | degrees | Positive = list to starboard. |
| `gz_at_30` | m | Righting lever at 30°; must be ≥ 0.20 m (IMO). |
| `area_0_30` | m·rad | Area under the GZ curve, 0–30°; must be ≥ 0.055 m·rad (IMO). |
| `imo_compliant` | flag | `1` only when all six IMO MSC.267(85) thresholds pass simultaneously. |
| `strength_compliant` | flag | `1` = pass, `0` = fail, `-1` = not checked (no `strength_limits` loaded). |
| `hydro_table_used` | flag | `1` = CSV table was used; `0` = box-hull fallback. Tells you which physics path produced the numbers. |

---

## Enums — the `OutputFormat` case

`cli.c` uses an `OutputFormat` enum to control how results are printed. Enums in C are named integer constants — they make code readable without requiring string parsing at every call site.

```c
/* From cli.c (output dispatch) */
typedef enum {
    FORMAT_HUMAN,
    FORMAT_JSON,
    FORMAT_CSV,
    FORMAT_TABLE,
    FORMAT_MARKDOWN
} OutputFormat;
```

The dispatch function `output_results` takes an `OutputFormat` value and calls the appropriate formatter. Using an enum instead of, say, a raw `int` means the compiler can warn if a `switch` misses a case — a small but real safety net.

!!! example "Enums versus defines"
    CargoForge-C uses `#define` for numeric constants (e.g. `MAX_FREE_RECTS 1024`, `MAX_LINE_LENGTH 256`) and `typedef enum` for sets of mutually exclusive named states. This is idiomatic C99: defines give you constants with no type; enums give you a named type the compiler can reason about.

---

## How the structs connect

```
Ship
 ├── Cargo cargo[]          ← heap array; cargo_count entries
 │    └── DGInfo_ *dg       ← heap-allocated only when DG: field present
 ├── HydroTable_ *hydro     ← NULL or heap; parsed from CSV
 ├── TankConfig_ *tanks     ← NULL or heap; parsed from CSV
 └── StrengthLimits_ *      ← NULL or heap; parsed from ship config keys
```

`ship_cleanup` is the single function responsible for unwinding this tree. It walks `cargo[]` freeing each `dg` pointer, then frees `cargo`, then frees the three optional extension pointers, and NULLs everything. A double-call to `ship_cleanup` is harmless because each `free` is guarded by a NULL check.

---

## Recap

- `Cargo` stores a single item: ID, weight in kg, dimensions in metres, cargo type, placed position (or `-1.0f` sentinel), and an optional heap pointer for DG data.
- `Ship` stores hull geometry, the dynamic `Cargo` array, lightship mass and KG, and three optional extension pointers that enable richer physics when present.
- The NULL-means-disabled pattern on `hydro`, `tanks`, and `strength_limits` lets the same struct model everything from a toy box ship to a fully instrumented vessel.
- `AnalysisResult` is a stack-allocated value type collecting all hydrostatic, stability, and strength outputs from a single call to `perform_analysis`.
- `OutputFormat` is a typical C enum: a named integer type the compiler can check in `switch` statements, used here to dispatch among five output renderers.
- All heap memory owned by `Ship` is released by a single `ship_cleanup` call; freeing the parts individually would leak or double-free.

*Next: [The preprocessor and headers](06-the-preprocessor-and-headers.md).*
