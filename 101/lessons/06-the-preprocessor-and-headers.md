# The preprocessor and headers

Before the C compiler sees a single line of your code, a separate program called the **preprocessor** runs first. It handles `#include`, `#define`, and `#ifndef` — the directives that begin with `#`. Understanding the preprocessor is what unlocks the `.h`/`.c` split that makes CargoForge-C's twelve source files work together as one coherent program.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Preprocessor** = "a find-and-replace robot that runs before the compiler even starts" — it pastes `#include` files in place, swaps every `MAX_FREE_RECTS` token for `1024`, and strips out `#ifdef` blocks the compiler should never see; the compiler only ever sees the finished result.
- **`#define` constant** = "a named number baked in at compile time, not stored in memory" — `MAX_HYDRO_ENTRIES 200` sets the fixed size of the `HydroEntry entries[MAX_HYDRO_ENTRIES]` array inside `HydroTable`; change one line in `hydrostatics.h` and every file that includes it picks up the new size automatically.
- **Include guard** = "a one-way door that lets a header be included many times but processed only once" — the `#ifndef CARGOFORGE_H / #define CARGOFORGE_H / ... / #endif` wrapper in `cargoforge.h` prevents the compiler from seeing the same `typedef` twice and throwing a redefinition error.
- **Declaration vs. definition** = "a declaration describes the shape; a definition is the real thing" — the prototype `AnalysisResult perform_analysis(const Ship *ship);` in `cargoforge.h` is a declaration; the actual function body with its `{ }` in `analysis.c` is the definition, and the linker wires the two together.
- **Forward declaration** = "just enough information to hold a pointer, nothing more" — `struct HydroTable_;` in `cargoforge.h` lets `Ship` carry a `struct HydroTable_ *hydro` pointer without pulling in all of `hydrostatics.h`; files that only hold the pointer never need to know `HydroTable`'s fields.
- **`static` on a file-scope function** = "private — the linker cannot see this symbol outside this `.c` file" — `static float lerp(...)` and `static void interpolate_entries(...)` in `hydrostatics.c` are invisible to every other translation unit, so they cannot clash with a `lerp` defined elsewhere and cannot be called accidentally from `parser.c` or `analysis.c`.

**Why it matters:** if you skip include guards, the compiler refuses to build the moment two files both include `cargoforge.h`; if you put a function *definition* in a header, every `.c` file that includes it produces its own copy of that symbol and the linker throws a "multiple definition" error — both failures halt the entire build of CargoForge-C.

---

## The mental model 🧠

You'll forget the directive syntax — hold THIS picture instead:

> Think of `cargoforge.h` as the ship's master manifest posted at the dock gate. Every crew member (`.c` file) reads the manifest to learn what cargo types exist, what the holds look like, and who to call for each job — but the manifest itself is not the cargo and not the crew. The actual cargo (`HydroTable` data, parsed numbers) lives below deck in `hydrostatics.c`; the actual workers (`parse_hydro_table`, `lerp`) are in the engine room. The gate has a stamp (`#ifndef CARGOFORGE_H`): once you've read the manifest, the stamp is marked and any second crew member who walks up gets waved through without re-reading it.

In CargoForge-C terms: `cargoforge.h` is the manifest — it declares the `Ship` struct, the `Cargo` shape, and prototypes like `parse_cargo_list` and `perform_analysis`, but defines nothing. The include guard is the gate stamp that prevents the compiler from choking on a duplicate `typedef Cargo` when both `analysis.c` and `parser.c` include the same header. The `static float lerp(...)` in `hydrostatics.c` is a private tool that never leaves the engine room — the linker cannot see it, so it cannot clash with anything else.

---

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>Preprocessor pipeline: how #include and #define expand before the compiler sees source code</title>
  <desc>A left-to-right flow showing cargoforge.h and hydrostatics.h being pasted into hydrostatics.c by the preprocessor, the compiler turning that into an object file, and the linker combining multiple object files into the final binary.</desc>

  <!-- Header files -->
  <rect x="10" y="20" width="130" height="48" rx="5" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="75" y="40" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">cargoforge.h</text>
  <text x="75" y="57" text-anchor="middle" font-size="9.5" fill="currentColor" opacity=".7">declarations + guard</text>

  <rect x="10" y="88" width="130" height="48" rx="5" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="75" y="108" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">hydrostatics.h</text>
  <text x="75" y="125" text-anchor="middle" font-size="9.5" fill="currentColor" opacity=".7">HydroTable + prototypes</text>

  <!-- Source file -->
  <rect x="10" y="158" width="130" height="48" rx="5" fill="none" stroke="currentColor" stroke-width="1.4" opacity=".6"/>
  <text x="75" y="178" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">hydrostatics.c</text>
  <text x="75" y="195" text-anchor="middle" font-size="9.5" fill="currentColor" opacity=".7">definitions + static lerp</text>

  <!-- Arrows to preprocessor -->
  <line x1="140" y1="44" x2="195" y2="110" stroke="currentColor" stroke-width="1.3" opacity=".5" marker-end="url(#arr)"/>
  <line x1="140" y1="112" x2="195" y2="118" stroke="currentColor" stroke-width="1.3" opacity=".5" marker-end="url(#arr)"/>
  <line x1="140" y1="182" x2="195" y2="130" stroke="currentColor" stroke-width="1.3" opacity=".5" marker-end="url(#arr)"/>

  <!-- Preprocessor box -->
  <rect x="200" y="80" width="110" height="68" rx="6" fill="none" stroke="currentColor" stroke-width="1.6" opacity=".8"/>
  <text x="255" y="105" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">Preprocessor</text>
  <text x="255" y="121" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">#include → paste</text>
  <text x="255" y="136" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">#define → replace</text>

  <!-- Arrow to compiler -->
  <line x1="310" y1="114" x2="355" y2="114" stroke="currentColor" stroke-width="1.5" opacity=".6" marker-end="url(#arr)"/>
  <text x="332" y="108" text-anchor="middle" font-size="8.5" fill="currentColor" opacity=".55">translation unit</text>

  <!-- Compiler box -->
  <rect x="360" y="80" width="100" height="68" rx="6" fill="none" stroke="currentColor" stroke-width="1.6" opacity=".8"/>
  <text x="410" y="105" text-anchor="middle" font-size="11" font-weight="600" fill="currentColor">Compiler</text>
  <text x="410" y="122" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">type-checks</text>
  <text x="410" y="137" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">generates code</text>

  <!-- Arrow to linker -->
  <line x1="460" y1="114" x2="505" y2="114" stroke="currentColor" stroke-width="1.5" opacity=".6" marker-end="url(#arr)"/>
  <text x="483" y="108" text-anchor="middle" font-size="8.5" fill="currentColor" opacity=".55">.o file</text>

  <!-- Linker box -->
  <rect x="510" y="80" width="100" height="68" rx="6" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="560" y="105" text-anchor="middle" font-size="11" font-weight="600" fill="#12A594">Linker</text>
  <text x="560" y="122" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">combines .o files</text>
  <text x="560" y="137" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">→ cargoforge binary</text>

  <!-- Private symbol note -->
  <rect x="200" y="200" width="200" height="40" rx="5" fill="none" stroke="#D05663" stroke-width="1.4" stroke-dasharray="5,3"/>
  <text x="300" y="218" text-anchor="middle" font-size="9.5" fill="#D05663" font-weight="600">static lerp() — linker-invisible</text>
  <text x="300" y="232" text-anchor="middle" font-size="9" fill="currentColor" opacity=".65">stays inside hydrostatics.o only</text>

  <!-- Arrow from compiler box down to private note -->
  <line x1="410" y1="148" x2="370" y2="200" stroke="#D05663" stroke-width="1.2" stroke-dasharray="4,3" opacity=".7" marker-end="url(#arrd)"/>

  <!-- Defs -->
  <defs>
    <marker id="arr" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="currentColor" opacity=".6"/>
    </marker>
    <marker id="arrd" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto">
      <path d="M0,0 L7,3.5 L0,7 Z" fill="#D05663" opacity=".7"/>
    </marker>
  </defs>

  <!-- Bottom label -->
  <text x="310" y="295" text-anchor="middle" font-size="9" fill="currentColor" opacity=".45">Each .c file is compiled independently; the linker resolves cross-file calls at the end.</text>
</svg>

## What the preprocessor actually does

The preprocessor is a text transformer. It reads your source file and produces a new text file — called a **translation unit** — that the compiler then compiles. Three directives cover the vast majority of what it does.

### `#include`: paste another file in place

```c
#include <stdio.h>
#include "hydrostatics.h"
```

The preprocessor replaces that line with the entire contents of the named file, verbatim, before compilation begins. Angle brackets (`<stdio.h>`) tell it to search the system include directories. Quoted paths (`"hydrostatics.h"`) tell it to search relative to the current file first, then fall back to system paths.

You never see this pasted result — it exists only in memory during compilation — but it is the foundation of modular C.

### `#define`: a text substitution rule

```c
#define MAX_LINE_LENGTH 256
#define MAX_DIMENSION   3
#define MAX_FREE_RECTS  1024
```

These three lines come from [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h). Every time the preprocessor encounters `MAX_FREE_RECTS` later in that translation unit, it replaces the token with `1024` before the compiler touches it. There is no variable, no memory address, no type — only text replacement.

Why use `#define` instead of a variable? Because the compiler can use the literal value for array sizes, `switch` cases, and other contexts that require a compile-time constant. In CargoForge-C, `MAX_FREE_RECTS = 1024` sets the size of the `spaces` array inside every `Bin3D` (the rectangular holds used by the 3D bin-packer). If you ever needed to increase it, changing one line in `cargoforge.h` would propagate to every file that includes it.

```c
#define MAX_HYDRO_ENTRIES 200
```

This constant, from [`include/hydrostatics.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/hydrostatics.h), caps how many draft rows a hydrostatic table may have. The `HydroEntry entries[MAX_HYDRO_ENTRIES]` array in `HydroTable` is sized at compile time using this value.

---

## Include guards: preventing double-inclusion

When multiple source files include the same header — and they always do — the preprocessor would paste the header's contents multiple times into the same translation unit. That causes **redefinition errors**: the compiler sees the same `typedef` twice and refuses.

Include guards prevent this:

```c
#ifndef CARGOFORGE_H
#define CARGOFORGE_H

/* ... all the content of the header ... */

#endif /* CARGOFORGE_H */
```

This is the first and last thing in [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h). The logic:

1. `#ifndef CARGOFORGE_H` — "if this symbol has not been defined yet …"
2. `#define CARGOFORGE_H` — "… define it now (so the next inclusion skips this block) …"
3. `#endif` — "… and end the conditional."

The second time any file tries to include `cargoforge.h`, `CARGOFORGE_H` is already defined, so the preprocessor skips everything between `#ifndef` and `#endif`. The guard symbol itself is just a name — by convention it mirrors the file name in uppercase with `.` replaced by `_`.

[`include/hydrostatics.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/hydrostatics.h) uses the same pattern:

```c
#ifndef HYDROSTATICS_H
#define HYDROSTATICS_H

#define MAX_HYDRO_ENTRIES 200

/* ... HydroEntry, HydroTable, function prototypes ... */

#endif /* HYDROSTATICS_H */
```

!!! note
    Modern compilers also support `#pragma once` as a shorter alternative, but it is not part of the C standard. CargoForge-C uses the portable `#ifndef` pattern throughout.

---

## Declaration vs. definition

This distinction is central to how `.h` and `.c` files divide responsibility.

A **declaration** tells the compiler that something exists and describes its shape — its name, type, and (for functions) its parameter and return types. A **definition** is where that thing actually lives: where memory is allocated for a variable, or where a function's body is written.

The rule: **a declaration can appear many times; a definition must appear exactly once.**

Headers contain declarations. Source files contain definitions.

### Function declarations in headers

From [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h):

```c
/* --- parser.c --- */
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);

/* --- analysis.c --- */
AnalysisResult perform_analysis(const Ship *ship);
void print_loading_plan(const Ship *ship);

/* --- ship cleanup --- */
void ship_cleanup(Ship *ship);
```

These are **function prototypes** — declarations without bodies. They tell every `.c` file that includes `cargoforge.h` exactly how to call `parse_ship_config`: it takes two arguments of those types and returns an `int`. The linker will later resolve the call to the actual compiled body in `parser.c`.

The corresponding **definitions** — the functions with their `{ }` bodies — live in the `.c` files, never in the header.

### Type declarations in headers

`typedef struct { ... } Cargo;` in `cargoforge.h` is also a declaration (of a type). It tells every translation unit what fields a `Cargo` struct contains. There is no memory allocated by a `typedef` — it only describes a shape. Memory is allocated when you write `Cargo c;` or `malloc(sizeof(Cargo))` in a `.c` file.

### Forward declarations: just the name, nothing more

[`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h) uses a more minimal form of declaration for types it needs to reference but does not fully define:

```c
struct HydroTable_;
struct TankConfig_;
struct StrengthLimits_;
struct DGInfo_;
```

These **forward declarations** tell the compiler "a struct with this tag exists." That is enough to declare a pointer to it — `struct HydroTable_ *hydro` inside `Ship` — without pulling in the full definition. The full `typedef struct HydroTable_ { ... } HydroTable;` lives in [`include/hydrostatics.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/hydrostatics.h), which only the files that actually use hydrostatic data need to include.

!!! tip
    This technique — forward-declaring a struct so you can hold a pointer without exposing its internals — is called an **opaque pointer**. It keeps compile-time dependencies minimal: a change to `HydroTable`'s fields only forces recompilation of files that include `hydrostatics.h`, not every file that includes `cargoforge.h`.

---

## How the `.h`/`.c` split enables modular compilation

CargoForge-C's source layout reflects this cleanly:

```
include/
  cargoforge.h      ← shared types and prototypes (the hub)
  hydrostatics.h    ← HydroTable, HydroEntry, prototypes
  tanks.h
  imdg.h
  ...
src/
  hydrostatics.c    ← definitions: parse_hydro_table, hydro_interpolate, ...
  analysis.c
  parser.c
  ...
```

Each `.c` file is compiled independently into an **object file** (`.o`). The linker then combines all the object files into the final binary. This matters because:

- **You only recompile what changed.** If you edit `hydrostatics.c`, only `hydrostatics.o` needs to be rebuilt — not `analysis.o` or `parser.o`.
- **Each `.c` file includes only the headers it needs.** [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c) begins with `#include "hydrostatics.h"` and the standard headers it uses (`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<math.h>`). It does not include `imdg.h` or `tanks.h`.
- **Circular inclusion is prevented by include guards.** `analysis.c` includes `cargoforge.h`, which forward-declares `HydroTable_`. It also includes `hydrostatics.h` to get the full type. Either order is safe because guards make both idempotent.

### Tracing one include chain

When [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c) is compiled, the translation unit the compiler actually sees begins like this (after preprocessing):

```
[contents of hydrostatics.h]
  → MAX_HYDRO_ENTRIES = 200
  → HydroEntry struct definition
  → HydroTable struct definition
  → prototypes: parse_hydro_table, hydro_interpolate, hydro_draft_from_displacement
[contents of stdio.h]   ← FILE *, fopen, fgets, etc.
[contents of stdlib.h]  ← malloc, free, etc.
[contents of string.h]  ← memset, strchr, sscanf, etc.
[contents of math.h]    ← (not used in hydrostatics.c but included)
[actual code of hydrostatics.c]
  → static float lerp(...)
  → static void interpolate_entries(...)
  → int parse_hydro_table(...)
  → int hydro_interpolate(...)
  → float hydro_draft_from_displacement(...)
```

The compiler reads that flattened stream. The functions are defined here. Other `.c` files that call `hydro_interpolate` include `hydrostatics.h` to get its prototype, and the linker wires the call site to this definition.

---

## `static`: keeping definitions private to one file

[`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c) defines two helper functions that are not declared in any header:

```c
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) { ... }
```

The `static` keyword on a function at file scope means: **this definition is invisible to the linker.** No other `.c` file can call `lerp` or `interpolate_entries` — they belong entirely to `hydrostatics.c`. This is the correct way to hide implementation details that do not belong in the public interface declared by the header.

Without `static`, the linker would see `lerp` as a globally visible symbol, which could conflict with a `lerp` in another translation unit (and several standard math libraries do define one).

---

## Putting it together: a concrete walk-through

The `HydroTable` type illustrates every concept in this lesson at once.

**In [`include/hydrostatics.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/hydrostatics.h):**
- `#define MAX_HYDRO_ENTRIES 200` — a compile-time constant
- `typedef struct { ... } HydroEntry;` — type declaration, no memory
- `typedef struct HydroTable_ { HydroEntry entries[MAX_HYDRO_ENTRIES]; int count; int loaded; } HydroTable;` — uses `MAX_HYDRO_ENTRIES` as an array size; `loaded` starts at 0 because `memset(table, 0, ...)` zeroes it
- Three function prototypes — declarations only

**In [`include/cargoforge.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/cargoforge.h):**
- `struct HydroTable_;` — forward declaration; enough for `struct HydroTable_ *hydro` inside `Ship`

**In [`src/hydrostatics.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/hydrostatics.c):**
- `#include "hydrostatics.h"` — pulls in all the declarations above
- `static float lerp(...)` — private helper, invisible outside this file
- `int parse_hydro_table(...)` — the definition matching the prototype; sets `table->loaded = 1` on success

When `perform_analysis` in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c) checks `if (ship->hydro != NULL && ship->hydro->loaded)`, it is reading the `loaded` flag that `parse_hydro_table` set. The compiler knew the type of `ship->hydro->loaded` because `analysis.c` includes both `cargoforge.h` (giving it the `Ship` type and the forward declaration of `HydroTable_`) and `hydrostatics.h` (giving it the full `HydroTable` definition).

---

## Recap

- The preprocessor runs before the compiler: it pastes `#include` files, expands `#define` tokens, and evaluates `#ifndef`/`#endif` guards — all as text substitution.
- Include guards (`#ifndef HEADER_H / #define HEADER_H / ... / #endif`) make headers safe to include multiple times without redefinition errors.
- Headers hold **declarations** (type shapes, function prototypes, constants); `.c` files hold **definitions** (function bodies, allocated storage).
- Forward-declaring a struct (`struct HydroTable_;`) lets you hold a pointer to it without including its full header, minimizing recompilation when implementation details change.
- `static` on a file-scope function restricts its visibility to that `.c` file, preventing linker conflicts and hiding implementation details.
- The `src/`+`include/` layout lets each `.c` file compile independently; the linker resolves cross-file references at the end.

*Next: [Strings and char arrays](07-strings-and-char-arrays.md).*
