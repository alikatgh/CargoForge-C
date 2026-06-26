# Glossary

Alphabetical reference for every term used across the CargoForge-C 101 course.
Terms are grouped by domain within each letter where it aids clarity, but the
overall order is strictly alphabetical by the bold entry word.

---

**AddressSanitizer (ASan)**
A compiler runtime that instruments every memory access and reports heap
overflows, use-after-free, and heap-use-after-free at the moment they occur.
CargoForge-C's `make test-asan` rebuilds with `-fsanitize=address` and runs the
full test suite; the fuzzer also runs under ASan with `abort_on_error=1` so any
violation produces a non-zero exit code that the fuzz harness treats as a
failure.

**AddressSanitizer — see also** *UndefinedBehaviorSanitizer*, *Sanitizer*.

**Analysis result**
The in-memory struct (`AnalysisResult` internally, `CfResult` in the public
library API) that `perform_analysis` returns after a full stability pass.  It
carries every computed quantity — draft, KG, KB, BM, GM, trim, heel, GZ curve
values, IMO compliance flag, and longitudinal-strength fields — keyed off the
loaded `Ship` and its placed cargo.

**Archimedes' principle**
A body immersed in a fluid is buoyed up by a force equal to the weight of the
fluid it displaces.  The entire draft-and-displacement chain in CargoForge-C
(`perform_analysis`, `hydro_draft_from_displacement`) rests on this principle:
the ship sinks until the weight of displaced seawater equals total displacement.

**Array**
A contiguous block of elements of the same type in C, accessed by index from 0
to `n-1`.  CargoForge-C uses arrays extensively: `cargo[]` inside `Ship`,
`spaces[]` inside `Bin3D`, and the hydrostatic-table row array inside
`HydroTable`.  C does not bounds-check arrays at runtime, making off-by-one
writes a common source of memory corruption.

**AABB (Axis-Aligned Bounding Box)**
A rectangular box whose edges are parallel to the coordinate axes, defined by an
origin and three extents.  `calculate_stack_pressure` in `constraints.c` uses
AABB overlap to find all placed cargo whose footprint overlaps a candidate
position, summing their weights to compute stacking pressure.

---

**Bin packing**
The combinatorial problem of fitting a set of items into containers (bins) while
respecting capacity constraints.  CargoForge-C solves a 3D variant: cargo items
are rectangular boxes; bins are named holds and the weather deck; the objective
is to place as many items as possible while honouring weight limits, point-load
limits, stacking pressure, and IMDG segregation rules.  See also *First Fit
Decreasing*, *Guillotine split*.

**Bin3D**
The C struct (`placement_3d.h`) representing one named cargo hold or deck area.
Fields: `name`, origin (`x/y/z`), dimensions (`width/depth/height`),
`max_weight`, `current_weight`, and an array of `Space3D` free rectangles.
`place_cargo_3d` creates three hard-coded bins — `ForwardHold`, `AftHold`, and
`Deck` — from the ship's length and maximum weight.

**Block coefficient (Cb)**
The ratio of a ship's underwater volume to the enclosing rectangular block
(L × B × T).  CargoForge-C uses `BLOCK_COEFF = 0.75` in the box-hull fallback
when computing draft from displaced volume: `draft = displaced_vol / (L × B × Cb)`.

**BM (transverse metacentric radius)**
The vertical distance from the centre of buoyancy B to the transverse metacentre
M, equal to the second moment of the waterplane area divided by displaced volume.
For the box-hull model: `BM = (L × B³ / 12 × Cw) / V`.  When a hydrostatic
table is loaded, BM is read directly from the interpolated `HydroEntry.bm` field.
BM is a geometric property of the hull form; a wider hull has a larger BM.

**Box-hull model**
A simplified ship geometry used when no hydrostatic CSV table is supplied.
Displacement, draft, KB, and BM are computed from the ship's length, breadth,
and fixed coefficients (`BLOCK_COEFF = 0.75`, `KB_FACTOR = 0.53`,
`WATERPLANE_COEFF = 0.85`).  Lessons 19 and 34 cover the model in detail.

**Buffer**
A region of memory used as temporary storage, typically a `char` array in C.
CargoForge-C uses fixed-size buffers for parsing (e.g. a 64-byte copy of the DG
field string in `parse_dg_field`) and for accumulating JSON output via
`open_memstream`.  Writing past the end of a buffer is *buffer overflow*, the
most exploitable class of C memory bug.

**Buffer overflow**
Writing data beyond the allocated end of a buffer, corrupting adjacent memory.
In C, there is no runtime check; the program continues with corrupted state.
CargoForge-C avoids this by using bounded copy functions (`strncpy`, `snprintf`)
and by validating string lengths before touching fixed-size fields like
`Cargo.id[32]` and `Cargo.type[16]`.

**Buoyancy**
The upward force a fluid exerts on a submerged or floating body, equal (by
Archimedes' principle) to the weight of displaced fluid.  In CargoForge-C,
buoyancy is modelled as a distributed load per hull station in
`distribute_buoyancy` (normalized to match total displacement) and as a
point-value lookup from the hydrostatic table at the computed draft.

---

**C99**
The 1999 revision of the C standard, adding variable-length arrays, `//`
comments, `<stdbool.h>`, designated initialisers, and `restrict`.
CargoForge-C compiles with `-std=c99 -D_POSIX_C_SOURCE=200809L`; the POSIX
macro unlocks `strtok_r`, `mkstemp`, and `open_memstream`.

**Cargo**
The `Cargo` struct (`cargoforge.h:39`) is the core data record for one cargo
item: a 32-char ID, weight in kg, three dimensions in metres, a type string, 3D
position floats (`pos_x/y/z`, set to -1.0 until placed), and an optional
heap-allocated `DGInfo *` pointer for dangerous-goods metadata.

**Centre of buoyancy (B / KB)**
The centroid of the underwater volume; the point through which the buoyant force
acts vertically upward.  KB (keel-to-centre-of-buoyancy) is the vertical
distance from keel to B.  In the box-hull model `KB = 0.53 × draft`; from the
hydrostatic table KB is interpolated directly.

**Centre of gravity (G / KG)**
The point through which the ship's total weight acts vertically downward.  KG
(keel-to-centre-of-gravity) is computed in CargoForge-C by summing the vertical
moment of lightship mass, every placed cargo item (weight × mid-height), and
every tank (weight × centre-of-liquid height), then dividing by total
displacement.

**Compilation unit**
A single `.c` source file processed by the compiler independently.  CargoForge-C
has one compilation unit per module (`parser.c`, `analysis.c`, `hydrostatics.c`,
etc.), linked together by the Makefile.  `static` functions and variables are
local to their compilation unit; they are invisible to other units.

**Constraints**
Rules that must hold for a cargo placement to be valid.  `check_cargo_constraints`
in `constraints.c` enforces six: point-load limit (5 t/m²), IMDG segregation,
legacy hazmat separation (3 m), stacking pressure (10 t/m², stricter for
fragile), reefer-deck preference (advisory), and deck weight ratio.

**Corpus (fuzzing)**
The seed set of inputs the fuzzer generates from.  CargoForge-C's `scripts/fuzz.sh`
defines arrays of adversarial values (negative numbers, overflow values, invalid
DG strings) that are assembled into random ship-config and cargo-manifest files.
The DG corpus includes both valid entries like `"DG:3.1:UN1203:A:F-E,S-D"` and
malformed entries like `"DG:::"` and `"DG:abc"` to exercise error paths.

---

**Dangling pointer**
A pointer that still holds the address of memory that has already been freed.
Dereferencing a dangling pointer is undefined behaviour and typically causes
heap-use-after-free.  The real bug fixed in CargoForge-C (`parser.c`) was
exactly this: an error path freed `ship->cargo` but left the pointer non-NULL,
so a later call to `ship_cleanup` dereferenced it again.  The fix NULLs the
pointer immediately after `free`.

**Deadweight (DWT)**
The total mass a ship can carry beyond its lightship weight, including cargo,
fuel, ballast, stores, and crew.  In CargoForge-C `Ship.max_weight` represents
the cargo deadweight limit (converted from tonnes to kg on parse); the
displacement used in stability calculations is lightship + cargo + tank liquid.

**Declaration vs. definition**
In C, a *declaration* introduces a name and type to the compiler without
allocating storage (e.g. `extern int x;` or a function prototype in a header);
a *definition* allocates storage or provides the function body.  CargoForge-C
header files (`cargoforge.h`, `libcargoforge.h`) hold declarations; `.c` files
hold definitions.

**Deployment (WASM)**
CargoForge-C can be compiled to WebAssembly via Emscripten (`make wasm`),
producing `cargoforge.js` and `cargoforge.wasm`.  Eight library functions are
exported; `ALLOW_MEMORY_GROWTH=1` allows the heap to grow at runtime.  This
enables the same C stability engine to run in a browser without a server.

**DG (dangerous goods)**
Cargo classified under the IMDG Code as posing a chemical, biological, or
physical hazard.  In CargoForge-C, a cargo item carries a heap-allocated
`DGInfo *` pointer when its manifest line includes a `DG:` field; NULL means
non-DG.  The IMDG segregation engine in `imdg.c` operates only on items where
`dg != NULL`.

**DGInfo**
The struct (`imdg.h`) that records IMDG metadata for one cargo item: `dg_class`
(1–9), `dg_division`, `un_number`, `stowage` category, and `ems` string.
It is heap-allocated by `parse_dg_field` and freed by `ship_cleanup` via
`cargo[i].dg`.

**Displacement**
The total mass of the ship plus everything on board, in tonnes.  By Archimedes'
principle this equals the mass of seawater displaced.  In CargoForge-C:
`displacement_t = (lightship_weight + cargo_weight + tank_weight) / 1000`.
Displacement drives all hydrostatic lookups and the free-surface correction
denominator.

**Draft (T)**
The vertical distance from the waterline to the lowest point of the keel.  In
CargoForge-C, draft is either computed from the box-hull formula
(`displaced_vol / (L × B × Cb)`) or inverse-interpolated from the hydrostatic
table (`hydro_draft_from_displacement`).  Draft gates every subsequent
hydrostatic lookup (KB, BM, MTC).

**Draft (T) — trim correction**
When the ship trims (bow/stern difference), the mean draft changes.  CargoForge-C
computes a single mean draft; trim is reported separately as `trim` (metres by
stern) and used to adjust the longitudinal centre-of-buoyancy lookup but not
the hydrostatic draft itself in the current model.

---

**EmS**
Emergency Schedule reference from the IMDG Code, stored in `DGInfo.ems` (e.g.
`"F-E,S-D"`).  The fire (`F-*`) and spillage (`S-*`) schedules tell crew which
emergency procedures to follow.  CargoForge-C stores the string verbatim; it does
not validate the EmS format beyond accepting it as the tail of the DG field.

**Enum**
A C type that defines a set of named integer constants.  CargoForge-C uses
`SegregationType` (SEG_NONE, SEG_AWAY_FROM, SEG_SEPARATED, etc.) and
`StowageCategory` (STOW_ANY, STOW_ON_DECK, STOW_UNDER_DECK) as enums, making
constraint code readable without magic numbers.

**Error path**
A code path reached when an operation fails (e.g. `safe_atof` returns NAN).  In
C, error paths commonly involve early returns and cleanup; forgetting to NULL out
a freed pointer on an error path is how CargoForge-C's real heap-use-after-free
bug was introduced.  Lesson 8 and the bug journal cover error-handling patterns.

---

**First Fit Decreasing (FFD)**
A bin-packing heuristic that sorts items largest-first and places each into the
first bin where it fits.  `place_cargo_3d` sorts cargo by volume descending
(`qsort` with `cargo_cmp_by_volume_desc`) before iterating; this tends to leave
small residual spaces that fit small items rather than large odd-shaped gaps.
Lesson 31 analyses FFD's approximation ratio and real-world performance.

**Float (C type)**
A 32-bit IEEE 754 single-precision floating-point number.  CargoForge-C uses
`float` for all physical quantities (weights, dimensions, hydrostatic values)
because the precision is sufficient for stability calculations and matches the
CSV source data.  `safe_atof` calls `strtof` (not `strtod`) and returns NAN on
range or parse failure.

**Free (memory)**
The C standard library function `free(ptr)` that returns a heap allocation to the
allocator.  After `free`, the pointer is dangling; any access through it is
undefined behaviour.  CargoForge-C's fix to the heap-use-after-free bug sets
`ship->cargo = NULL` immediately after every `free(ship->cargo)` call so that
`ship_cleanup`'s `if (ship->cargo)` guard makes the second access a no-op.

**Free surface (effect)**
When a tank is partially filled with liquid, any roll of the ship causes the
liquid surface to shift, moving the liquid's centre of gravity outward and
reducing effective GM.  The correction is the sum of `ρ × l × b³ / 12` for each
partial tank, divided by displacement, added to virtual KG.  CargoForge-C
computes this in `calculate_virtual_kg_rise` (`tanks.c`) and subtracts it from
GM to yield `gm_corrected`.

**Free-surface correction (FSC)**
The numerical value of the virtual KG rise due to free surfaces, in metres.
`GM_corrected = GM - FSC`.  All IMO criteria in CargoForge-C use `gm_corrected`,
not raw GM.

**Fuzzing**
Automated testing that feeds a program large volumes of randomly mutated or
generated inputs looking for crashes and sanitizer violations.  CargoForge-C's
`scripts/fuzz.sh` runs 300 iterations by default, each constructing a random
ship config and cargo manifest, running `cargoforge optimize` or `validate`
under ASan+UBSan, and treating exit code ≥ 128 (signal) or sanitizer output
as failure.  The real heap-use-after-free bug was discovered this way.

---

**GZ (righting lever)**
The horizontal distance between the line of action of buoyancy and the line of
action of gravity when the ship is heeled to angle θ.  A positive GZ produces a
righting moment that returns the ship to upright.  CargoForge-C uses the
wall-sided formula: `GZ(θ) = sin(θ) × (GM + BM × tan²(θ) / 2)`.

**GZ curve**
A plot of GZ against heel angle, used to assess dynamic stability.  IMO intact
stability criteria (MSC.267/85) specify minimum areas under the curve and a
minimum GZ at 30°.  CargoForge-C computes `gz_at_30`, `gz_max`, `gz_max_angle`,
`area_0_30`, `area_0_40`, and `area_30_40` from the wall-sided formula via
trapezoidal integration over 100 steps.

**Guillotine split**
A space-partitioning technique used in 2D and 3D bin-packing: after placing an
item, the remaining free space is cut by axis-aligned planes into (up to three)
new free rectangles — right remainder, back remainder, top remainder.
`split_space_3d` in `placement_3d.c` implements this, appending new `Space3D`
entries to the bin's free-space list.

**GM (metacentric height)**
The vertical distance from the centre of gravity G to the transverse metacentre M.
`GM = KB + BM - KG`.  A positive GM means the ship is stable; a negative GM means
it will capsize without external support.  IMO requires `GM_corrected ≥ 0.15 m`.
In CargoForge-C, `gm_corrected` (after free-surface deduction) is the value used
for all criteria checks and heel/GZ computations.

---

**Header file**
A `.h` file in C that contains declarations shared between compilation units:
struct definitions, function prototypes, `#define` constants, and `typedef`
aliases.  CargoForge-C's public API is declared in `libcargoforge.h`; internal
module interfaces in `cargoforge.h`, `hydrostatics.h`, `tanks.h`, `imdg.h`,
etc.  Headers are included via `#include`; include guards (`#ifndef …`) prevent
double-inclusion.

**Heap**
The region of memory managed by `malloc`/`free` for dynamic allocation.
CargoForge-C heap-allocates the `cargo` array (`Ship.cargo`), each `DGInfo`
struct, hydrostatic tables, tank configs, and strength-limits structs.  Heap
memory persists until explicitly freed; failing to free it is a *memory leak*.

**Heap-use-after-free**
Accessing heap memory after it has been freed.  The canonical bug in
CargoForge-C: `parse_cargo_list` freed `ship->cargo` on an error path but left
`ship->cargo_count` non-zero, so the later `ship_cleanup` iterated over the
freed array and read `cargo[i].dg`.  AddressSanitizer detected this as
`heap-use-after-free` at exit code 134.

**Hogging**
A longitudinal bending mode where the midship section is pushed upward relative
to the ends — the hull curves concave-down like a hog's back.  In CargoForge-C,
`permissible_bm_hog` is the class-society limit on hogging bending moment.
Positive SWBM values in `LongStrengthResult` correspond to hogging.

---

**IMDG Code**
International Maritime Dangerous Goods Code, the IMO framework governing
classification, packaging, marking, documentation, stowage, and segregation of
dangerous goods at sea.  CargoForge-C implements Table 7.2.4 of the IMDG Code
as a 17×17 segregation matrix in `imdg.c`.  Classes 1–9 (with subdivisions) are
supported.

**IMO (International Maritime Organization)**
The United Nations agency responsible for international shipping regulations.
CargoForge-C's intact stability criteria come from IMO resolution MSC.267(85)
(the IS Code 2008), Parts A Chapter 2.2.  The six thresholds are constants
prefixed `IMO_` in `analysis.c`.

**IMO intact stability criteria**
Six quantitative thresholds that a vessel's GZ curve must satisfy, per
MSC.267(85): GM ≥ 0.15 m, GZ at 30° ≥ 0.20 m, angle of max GZ ≥ 25°,
area 0–30° ≥ 0.055 m·rad, area 0–40° ≥ 0.090 m·rad, area 30–40° ≥ 0.030 m·rad.
`imo_compliant = 1` in `AnalysisResult` only when all six pass.

**Include guard**
A preprocessor pattern (`#ifndef HEADER_H / #define HEADER_H / … / #endif`)
that prevents a header from being processed more than once per compilation unit.
All CargoForge-C headers use include guards to avoid duplicate-declaration errors
when multiple `.c` files include the same header chain.

---

**JSON-RPC**
A remote procedure call protocol using JSON as the message format.  CargoForge-C's
`cargoforge serve` subcommand starts a single-threaded POSIX socket server that
accepts HTTP POST requests whose body is a JSON-RPC 2.0 object.  Supported
methods: `optimize`, `validate`, `version`.

---

**KB — see** *Centre of buoyancy*.

**KG — see** *Centre of gravity*.

---

**LCG (longitudinal centre of gravity)**
The fore-aft position of the ship's combined centre of gravity, measured from
midship.  Positive = aft in CargoForge-C.  `LCG` drives the trim calculation:
`trim = LCG × L / GM_L`.

**Lightship**
The displacement of the ship itself — hull, machinery, fixed equipment — with no
cargo, fuel, ballast, or stores on board.  `Ship.lightship_weight` (kg) and
`Ship.lightship_kg` (m, vertical KG of lightship) are mandatory config keys.
The lightship KG contributes the first term in the vertical-moment sum.

**Linear interpolation**
Estimating a value between two known data points by assuming the function is a
straight line between them.  `hydro_interpolate` and `hydro_draft_from_displacement`
in `hydrostatics.c` use linear interpolation on the hydrostatic table to obtain
draft, KB, BM, and MTC at any displacement or draft within the table's range.

**Longitudinal strength**
The ability of the hull girder to resist bending and shear forces along its
length, caused by the mismatch between weight and buoyancy distributions.
CargoForge-C calculates still-water shear force (SWSF) and bending moment (SWBM)
at 20 hull stations in `longitudinal_strength.c` and compares them to
class-society permissible limits.

---

**Macro**
A `#define` preprocessor token substitution.  CargoForge-C uses macros for
constants (`IMO_GM_MIN`, `BLOCK_COEFF`, `MAX_POINT_LOAD`, `MIN_HAZMAT_SEPARATION`,
`MAX_FREE_RECTS`) and for the library version string (`CF_VERSION_STRING`).
Function-like macros are avoided in favour of `static inline` functions or
regular functions to preserve type safety.

**Malloc**
`malloc(n)` allocates `n` bytes on the heap and returns a pointer, or NULL on
failure.  CargoForge-C allocates `DGInfo` structs in `parse_dg_field`,
`ship->cargo` arrays in `parse_cargo_list`, and hydrostatic/tank/strength structs
in their respective parsers.  Every `malloc` call is paired with a corresponding
`free` path in `ship_cleanup`.

**Memory leak**
Heap memory that is allocated but never freed, causing the process's memory
footprint to grow.  CargoForge-C uses `ship_cleanup` as the single teardown
function that frees all heap-allocated members of `Ship`; the library wraps this
in `cargoforge_close`.

**Metacentre (M)**
The point about which a slightly heeled ship rotates, located a distance BM above
the centre of buoyancy B.  For small angles the metacentre is effectively fixed;
for larger angles (handled by the wall-sided formula) M rises as the hull form
changes.  A ship is initially stable if G is below M, i.e. GM > 0.

---

**NaN (Not a Number)**
A special IEEE 754 floating-point value indicating an invalid result.  CargoForge-C's
`safe_atof` returns NAN when a field is out of range or unparseable.  Callers
check `isnan(value)` to detect parse errors without needing separate error codes.
`json_output.c` emits `null` for all hydrostatic fields when `result->gm` is NAN,
which occurs when the ship is overweight.

**Null pointer**
A pointer whose value is zero (or `NULL`), guaranteed to compare unequal to any
valid object pointer.  Dereferencing NULL is undefined behaviour (and causes a
segfault on most platforms).  CargoForge-C uses NULL as the sentinel for optional
struct pointers (`Ship.hydro`, `Ship.tanks`, `Ship.strength_limits`, `Cargo.dg`)
so that code can check `if (ptr)` before using it.

---

**Opaque handle**
A pointer to an incomplete struct type whose fields are hidden from callers.
`CargoForge *` in `libcargoforge.h` is an opaque handle: the internal struct
`CargoForge` (with its `Ship`, flags, and caches) is defined only in
`libcargoforge.c`.  Callers interact only through the public API functions, which
gives CargoForge-C ABI stability across version changes.

**Orientation (3D placement)**
One of the six axis-aligned rotations of a rectangular box (all permutations of
length, width, height).  `get_orientation_dims` in `placement_3d.c` enumerates
all six; `find_best_fit_3d` tries all six for each candidate space and picks the
one that minimises wasted volume.

---

**Parser**
Code that converts raw text (config files, cargo manifests, CSV tables) into C
data structures.  CargoForge-C's `parser.c` handles ship config (key=value),
cargo manifests (whitespace-delimited), and DG field grammar.  `hydrostatics.c`
and `tanks.c` each contain their own CSV parsers.  `safe_atof` is the shared
numeric-field parser used by all of them.

**Point load**
The downforce per unit area exerted by a cargo item on the deck or cargo beneath
it, in t/m².  Computed as `weight_t / (length_m × width_m)`.  If point load
exceeds `MAX_POINT_LOAD` (5 t/m²), `check_cargo_constraints` rejects the
placement.  High point loads can deform container floors and hatch covers.

**Pointer**
A C variable that stores the memory address of another object.  Pointers enable
dynamic allocation, linked data structures, and pass-by-reference semantics.
CargoForge-C uses pointers pervasively: `Cargo *`, `Ship *`, `DGInfo *`,
function arguments like `HydroEntry *result`.  Incorrect pointer arithmetic or
failing to NULL a freed pointer are the two most common C memory bugs.

**Preprocessor**
The first phase of C compilation, which handles `#include`, `#define`, and
`#ifdef` directives before the compiler sees any code.  CargoForge-C uses the
preprocessor for include guards, physical constants, and the `CF_VERSION_STRING`
macro.  Lesson 6 covers the preprocessor in depth.

---

**Reefer**
Refrigerated cargo requiring a controlled-temperature environment, typically
carried in insulated containers connected to ship's power.  In CargoForge-C,
`Cargo.type = "reefer"` triggers an advisory constraint in
`check_cargo_constraints`: reefer cargo placed outside the `Deck` bin generates
a warning (but is not rejected) because holds may lack reefer plug points.

**Righting moment**
The torque that returns a heeled ship to upright, equal to `displacement × GZ`
at heel angle θ.  A positive righting moment at all angles up to some range
means the ship is self-righting; the IMO criteria encode minimum righting moment
thresholds via minimum GZ values and minimum areas under the GZ curve.

---

**safe_atof**
A CargoForge-C utility function (`parser.c`) that wraps `strtof` with range
validation.  Signature: `float safe_atof(const char *s, float min, float max,
const char *field_name)`.  Returns NAN and prints an error message on failure.
Used for every numeric field in every parser, providing a single chokepoint for
numeric input validation.

**Sagging**
The opposite of hogging: midship is pushed downward relative to the ends — the
hull curves concave-up.  Negative SWBM values in `LongStrengthResult` correspond
to sagging.  `permissible_bm_sag` is the class-society sagging limit.

**Sanitizer**
A compiler-inserted runtime checker for a specific class of bugs.  CargoForge-C
uses two: AddressSanitizer (ASan) for memory errors and UndefinedBehaviorSanitizer
(UBSan) for C undefined-behaviour violations.  Both are enabled together with
`-fsanitize=address,undefined`.

**Segregation (IMDG)**
The minimum required physical separation between incompatible dangerous goods,
specified in IMDG Table 7.2.4 as one of five categories: `SEG_NONE` (0 m),
`SEG_AWAY_FROM` (3 m), `SEG_SEPARATED` (6 m), `SEG_SEPARATED_COMPLETE` (12 m),
`SEG_SEPARATED_LONG` (24 m), or `SEG_INCOMPATIBLE` (forbidden co-loading).
`imdg.c` maps class+division pairs to `SegregationType` via a 17×17 matrix and
`class_to_index`.

**Sentinel value**
A special out-of-band value used to signal a particular state.  CargoForge-C
uses `-1.0f` as the sentinel for "not yet placed" in `Cargo.pos_x/y/z`; analysis
code checks `pos_x < 0` to exclude unplaced items.  NULL serves as the sentinel
for optional struct pointers.

**Ship (struct)**
The central data structure (`cargoforge.h:53`) holding all ship parameters and
the cargo manifest: dimensions, weight limits, lightship data, cargo array,
optional hydrostatic table, optional tank config, and optional strength limits.
`parse_ship_config` populates it; `perform_analysis` reads it; `ship_cleanup`
frees its heap members.

**Stack (call stack)**
The region of memory that holds function call frames — local variables, return
addresses, and saved registers.  Stack memory is automatically allocated on
function entry and freed on return.  CargoForge-C's parsing functions use stack
buffers for small temporaries (e.g. 256-byte key/value buffers in
`parse_ship_config`).  Stack overflow (too-deep recursion or large local arrays)
is a harder-to-detect bug than heap overflow.

**Stowage category**
An IMDG designation controlling where dangerous goods may be stowed: `A`
(anywhere — `STOW_ANY`), `D` (on deck — `STOW_ON_DECK`), or `U` (under deck —
`STOW_UNDER_DECK`).  `parse_dg_field` maps the single-letter code to
`StowageCategory`; `check_cargo_constraints` enforces deck vs. hold preference.

**strtok_r**
The POSIX reentrant string-tokeniser, splitting a string on delimiter characters
without modifying a hidden global state variable (unlike `strtok`).  CargoForge-C
uses `strtok_r` in `parse_dg_field` to split the `DG:class:UN:stow:EmS` field
on `':'`.  Enabled by `-D_POSIX_C_SOURCE=200809L`.

**Struct**
A C aggregate type grouping named fields of potentially different types.
CargoForge-C's data model is expressed almost entirely as structs: `Cargo`,
`Ship`, `DGInfo`, `HydroEntry`, `Tank`, `Bin3D`, `Space3D`, `AnalysisResult`,
`CfResult`, `StrengthLimits`, `IMDGCheckResult`.  Structs are passed by pointer
for efficiency and to allow callers to observe mutations.

**SWBM (still-water bending moment)**
The longitudinal bending moment in a ship at rest in calm water, arising from
the mismatch between the weight and buoyancy distributions.  CargoForge-C
integrates shear force a second time to obtain SWBM at each of 20 hull stations
and reports the maximum (the larger of hog and sag) in `AnalysisResult.max_bending_moment`.

**SWSF (still-water shear force)**
The longitudinal shear force at a hull cross-section in calm water.  Computed by
integrating net load (weight minus buoyancy) from AP to FP.
`AnalysisResult.max_shear_force` holds the peak SWSF value (tonnes).

---

**TCG (transverse centre of gravity)**
The lateral offset of the ship's combined centre of gravity from the centreline,
in metres.  `TCG = avg_y - B/2`.  A non-zero TCG causes a heeling moment;
CargoForge-C computes steady-state heel as `atan(TCG / GM_corrected)`.

**Tokeniser**
Code that splits an input string into a sequence of tokens (meaningful substrings).
CargoForge-C's cargo manifest parser splits lines on whitespace using `sscanf` to
extract the four mandatory fields; `parse_dg_field` uses `strtok_r` to split the
DG grammar on `':'`.  Lesson 27 covers tokenising strategies.

**TPC (tonnes per centimetre)**
The mass that must be added to or removed from a ship to change its mean draft by
one centimetre.  Read from the hydrostatic table (`HydroEntry.tpc`) at the
interpolated draft.  Used indirectly in CargoForge-C for trim calculations via MTC.

**Trim**
The difference in draft between the stern and the bow; positive trim (trim by
stern) means the stern is deeper.  CargoForge-C computes trim as
`LCG × L / GM_L` where `GM_L` is the longitudinal metacentric height derived
from MTC.  `AnalysisResult.trim` is in metres by stern.

---

**UBSan (UndefinedBehaviorSanitizer)**
A compiler runtime that traps C undefined-behaviour violations — signed integer
overflow, null-pointer dereference, misaligned access, out-of-bounds array
indexing (where detectable at compile time) — at the exact moment they occur.
Enabled with `-fsanitize=undefined`.  CargoForge-C runs UBSan alongside ASan in
`make test-asan` and in `scripts/fuzz.sh`.

**UN number**
A four-digit United Nations number uniquely identifying a dangerous substance or
article under the IMDG Code (e.g. `UN1203` for petrol/gasoline).  Stored in
`DGInfo.un_number` as a string; CargoForge-C stores it verbatim without
validating the number against any lookup table.

**Undefined behaviour (UB)**
Any program action that the C standard does not define — signed overflow,
use-after-free, reading uninitialised memory, null dereference, out-of-bounds
access.  The compiler is free to assume UB never happens and may optimise in ways
that produce surprising results or security vulnerabilities.  CargoForge-C's
sanitizer suite detects UB at runtime during testing.

**Use-after-free**
Accessing memory (on the stack or heap) after its lifetime has ended.  On the
heap this is *heap-use-after-free*; on the stack it is *stack-use-after-return*
(returning a pointer to a local variable).  Both are undefined behaviour.
See *heap-use-after-free* for the specific bug found and fixed in CargoForge-C.

---

**Valgrind**
A dynamic binary instrumentation tool that detects memory errors (leaks,
use-after-free, invalid reads/writes) without recompilation.  `make test-valgrind`
runs each CargoForge-C test binary and an integration run through Valgrind with
`--leak-check=full --error-exitcode=1`.  Valgrind is slower than ASan but does
not require recompilation of the target binary.

---

**Wall-sided formula**
An approximation for the righting lever GZ valid for ships with vertical sides
at the waterplane: `GZ(θ) = sin(θ) × (GM + BM × tan²(θ) / 2)`.  It improves on
the small-angle approximation (`GZ ≈ GM × sin(θ)`) for angles up to roughly 30°.
`gz_at_angle` in `analysis.c` implements this formula; `integrate_gz` uses it
over 100 trapezoidal steps to compute righting-lever areas.

**Waterplane area coefficient (Cw)**
The ratio of the actual waterplane area to the circumscribed rectangle (L × B).
CargoForge-C uses `WATERPLANE_COEFF = 0.85` in the box-hull BM formula.  From
the hydrostatic table, waterplane area is available directly in `HydroEntry.waterplane_area`.

---

*End of glossary. For the full lesson sequence, see [index.md](index.md).*
