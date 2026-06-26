# Structs, enums, and the data model

Every CargoForge-C operation — parsing a manifest, running stability analysis, placing cargo in a hold — ultimately reads or writes two core structs: `Cargo` and `Ship`. Understanding their fields is understanding the program. This lesson walks each field in detail, explains the units, shows how optional extensions hang off the struct as pointers, and introduces the enums that control output behavior.

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
