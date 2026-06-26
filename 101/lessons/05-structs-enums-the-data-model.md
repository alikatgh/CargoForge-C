# Structs, enums, and the data model

Every CargoForge-C operation — parsing a manifest, running stability analysis, placing cargo in a hold — ultimately reads or writes two core structs: `Cargo` and `Ship`. Understanding their fields is understanding the program. This lesson walks each field in detail, explains the units, shows how optional extensions hang off the struct as pointers, and introduces the enums that control output behavior.

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

## The mental model 🧠

You'll forget the field names — hold THIS picture instead:

> Imagine a superintendent's clipboard hanging on a hook in the cargo office. Each row on the clipboard is one crate: an ID sticker, a weight scrawled in pencil, a box drawn for dimensions, and — only if the crate is hazardous — a red DG card clipped to the row. Until the crane sets the crate in the hold, the "position" column is blank (that's `pos_x = -1.0f`). The clipboard itself hangs on a bigger board labelled with the ship's length, breadth, and deadweight — that's `Ship`. Three extra folders dangle beneath it: a hydrostatic table binder, a tank diagram, and a strength-limits card. If the folders are missing, the office uses rough estimates instead.

In code, `Cargo` is one clipboard row. `Ship` wraps the whole board: it owns the dynamic array of rows (`Cargo *cargo`), the hull numbers (`length`, `width`, `max_weight`), and the three optional folders (`hydro`, `tanks`, `strength_limits`). When `perform_analysis` walks the rows to compute KG, it skips any row whose position column is still blank — exactly as a superintendent would skip a crate still sitting on the dock. `ship_cleanup` is the act of filing everything away at voyage end: it empties each row's DG card, then the rows, then the folders, in one tidy pass.

---

<svg viewBox="0 0 620 340" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>CargoForge-C data model: Ship owns Cargo array, Cargo optionally owns DGInfo, Ship optionally owns HydroTable/TankConfig/StrengthLimits</title>
  <desc>Box-and-arrow diagram showing the Ship struct containing hull fields and a pointer to a heap Cargo array. Each Cargo entry holds id, weight, dimensions, type, pos_x/y/z, and an optional dg pointer. Ship also holds three nullable extension pointers: hydro, tanks, and strength_limits.</desc>

  <!-- Ship outer box -->
  <rect x="10" y="10" width="200" height="320" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.6" stroke-opacity="0.9"/>
  <text x="110" y="30" text-anchor="middle" font-size="12" font-weight="700"
        fill="#12A594">Ship</text>

  <!-- Ship fields -->
  <text x="22" y="52" font-size="10" fill="currentColor" opacity="0.85">length (m)</text>
  <text x="22" y="68" font-size="10" fill="currentColor" opacity="0.85">width (m)</text>
  <text x="22" y="84" font-size="10" fill="currentColor" opacity="0.85">max_weight (kg)</text>
  <text x="22" y="100" font-size="10" fill="currentColor" opacity="0.85">lightship_weight (kg)</text>
  <text x="22" y="116" font-size="10" fill="currentColor" opacity="0.85">lightship_kg (m)</text>
  <text x="22" y="132" font-size="10" fill="currentColor" opacity="0.85">cargo_count / cargo_capacity</text>

  <!-- cargo pointer row -->
  <rect x="14" y="142" width="182" height="20" rx="3"
        fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1"/>
  <text x="22" y="156" font-size="10" font-weight="600" fill="#12A594">Cargo *cargo  →</text>

  <!-- extension pointer rows -->
  <text x="22" y="186" font-size="10" fill="currentColor" opacity="0.7">HydroTable_ *hydro</text>
  <text x="22" y="204" font-size="10" fill="currentColor" opacity="0.7">TankConfig_ *tanks</text>
  <text x="22" y="222" font-size="10" fill="currentColor" opacity="0.7">StrengthLimits_ *limits</text>
  <text x="22" y="244" font-size="10" fill="currentColor" opacity="0.45">(NULL = disabled)</text>

  <!-- ship_cleanup label -->
  <text x="22" y="292" font-size="9" fill="currentColor" opacity="0.5">▼ ship_cleanup()</text>
  <text x="22" y="306" font-size="9" fill="currentColor" opacity="0.5">  frees dg[], cargo, extensions</text>

  <!-- Arrow from cargo pointer to Cargo array box -->
  <line x1="196" y1="152" x2="250" y2="152" stroke="#12A594" stroke-width="1.5" marker-end="url(#arr-teal)"/>

  <!-- Cargo array box -->
  <rect x="250" y="10" width="190" height="240" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.6" stroke-opacity="0.9"/>
  <text x="345" y="30" text-anchor="middle" font-size="12" font-weight="700"
        fill="#12A594">Cargo[i]</text>
  <text x="345" y="46" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.5">(heap array)</text>

  <text x="262" y="66"  font-size="10" fill="currentColor" opacity="0.85">id[32]  — e.g. "HeavyMachinery"</text>
  <text x="262" y="82"  font-size="10" fill="currentColor" opacity="0.85">weight  — kg (tonnes × 1000)</text>
  <text x="262" y="98"  font-size="10" fill="currentColor" opacity="0.85">dimensions[3]  — L × W × H (m)</text>
  <text x="262" y="114" font-size="10" fill="currentColor" opacity="0.85">type[16]  — "standard" | "reefer" …</text>

  <!-- pos fields with sentinel callout -->
  <text x="262" y="134" font-size="10" fill="currentColor" opacity="0.85">pos_x  — m from hull origin</text>
  <text x="262" y="150" font-size="10" fill="currentColor" opacity="0.85">pos_y  — m from hull origin</text>
  <text x="262" y="166" font-size="10" fill="currentColor" opacity="0.85">pos_z  — m from keel</text>

  <!-- sentinel callout box -->
  <rect x="262" y="176" width="170" height="28" rx="3"
        fill="#D05663" fill-opacity="0.10" stroke="#D05663" stroke-width="1"/>
  <text x="270" y="189" font-size="9" fill="#D05663">pos_x = −1.0f → not yet placed</text>
  <text x="270" y="200" font-size="9" fill="#D05663">skipped by perform_analysis</text>

  <!-- dg pointer row -->
  <rect x="256" y="212" width="178" height="20" rx="3"
        fill="currentColor" fill-opacity="0.06" stroke="currentColor" stroke-opacity="0.3" stroke-width="1"/>
  <text x="262" y="226" font-size="10" fill="currentColor" opacity="0.7">DGInfo_ *dg  →</text>

  <!-- Arrow from dg to DGInfo box -->
  <line x1="440" y1="222" x2="490" y2="222" stroke="currentColor" stroke-opacity="0.5"
        stroke-width="1.2" stroke-dasharray="4,3" marker-end="url(#arr-grey)"/>

  <!-- DGInfo box -->
  <rect x="490" y="198" width="118" height="68" rx="6"
        fill="none" stroke="currentColor" stroke-opacity="0.5" stroke-width="1.2"/>
  <text x="549" y="216" text-anchor="middle" font-size="11" font-weight="600"
        fill="currentColor" opacity="0.75">DGInfo_</text>
  <text x="500" y="233" font-size="9" fill="currentColor" opacity="0.6">un_number</text>
  <text x="500" y="247" font-size="9" fill="currentColor" opacity="0.6">hazard_class</text>
  <text x="500" y="261" font-size="9" fill="currentColor" opacity="0.5">(heap; NULL if</text>
  <text x="500" y="272" font-size="9" fill="currentColor" opacity="0.5"> no DG: field)</text>

  <!-- Extension boxes (right of Ship, lower) -->
  <rect x="250" y="268" width="118" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.4" stroke-width="1"/>
  <text x="309" y="286" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.65">HydroTable_</text>

  <rect x="380" y="268" width="118" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.4" stroke-width="1"/>
  <text x="439" y="286" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.65">TankConfig_</text>

  <rect x="490" y="268" width="118" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-opacity="0.4" stroke-width="1"/>
  <text x="549" y="286" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.65">StrengthLimits_</text>

  <!-- Dashed arrows from Ship extension fields to the boxes -->
  <line x1="196" y1="186" x2="309" y2="268" stroke="currentColor" stroke-opacity="0.3"
        stroke-width="1" stroke-dasharray="4,3"/>
  <line x1="196" y1="204" x2="439" y2="268" stroke="currentColor" stroke-opacity="0.3"
        stroke-width="1" stroke-dasharray="4,3"/>
  <line x1="196" y1="222" x2="549" y2="268" stroke="currentColor" stroke-opacity="0.3"
        stroke-width="1" stroke-dasharray="4,3"/>

  <!-- NULL / optional label -->
  <text x="310" y="314" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.4">NULL = disabled (box-hull fallback)</text>

  <!-- Arrow markers -->
  <defs>
    <marker id="arr-teal" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="#12A594"/>
    </marker>
    <marker id="arr-grey" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L0,6 L8,3 z" fill="currentColor" opacity="0.45"/>
    </marker>
  </defs>
</svg>

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
