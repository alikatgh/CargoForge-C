# The preprocessor and headers

Before the C compiler sees a single line of your code, a separate program called the **preprocessor** runs first. It handles `#include`, `#define`, and `#ifndef` — the directives that begin with `#`. Understanding the preprocessor is what unlocks the `.h`/`.c` split that makes CargoForge-C's twelve source files work together as one coherent program.

## The mental model 🧠

The **preprocessor** is a copy-paste robot that runs before the real compiler ever looks at your code. `#include "cargoforge.h"` means, literally, *paste the entire contents of that file right here.* `#define MAX_HYDRO_ENTRIES 200` means *find every one of those words and swap in `200`.* The compiler only ever sees the finished, pasted-together result.

Headers are the **shared blueprint**. `cargoforge.h` says what a `Ship` *is* and what `perform_analysis` *looks like*; every `.c` file pastes that blueprint in so they all agree, then each `.c` is the actual factory that builds its part. The prototype on the blueprint is a *declaration* (just the shape); the function body with its `{ }` in the factory is the *definition* (the real thing) — and the linker wires the two together.

The **include guard** (`#ifndef CARGOFORGE_H …`) is the robot's note to itself: *if you've already pasted me once, skip it.* Without it, two includes of the same header would define `Ship` twice and the compiler would choke on the redefinition. One blueprint, pasted everywhere, processed once each.

<svg viewBox="0 0 600 240" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The preprocessor pastes cargoforge.h into every .c file before compiling</title>
<desc>cargoforge.h is the shared blueprint. The preprocessor copies its full contents into parser.c, analysis.c, and main.c, so each translation unit agrees on what a Ship is. The compiler then builds each and the linker joins them into one cargoforge binary.</desc>
<rect x="14" y="92" width="120" height="56" rx="5" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.2"/>
<text x="74" y="116" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">cargoforge.h</text>
<text x="74" y="132" font-size="9" text-anchor="middle" fill="currentColor" opacity="0.55">shared blueprint</text>
<rect x="226" y="34" width="128" height="40" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="290" y="58" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">parser.c</text>
<rect x="226" y="100" width="128" height="40" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="290" y="124" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">analysis.c</text>
<rect x="226" y="166" width="128" height="40" rx="4" fill="currentColor" fill-opacity="0.05" stroke="currentColor" stroke-opacity="0.4"/><text x="290" y="190" font-size="11" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">main.c</text>
<line x1="134" y1="112" x2="224" y2="54" stroke="#12A594" stroke-opacity="0.6"/><path d="M217,53 L224,54 L220,60" fill="none" stroke="#12A594" stroke-opacity="0.7"/>
<line x1="134" y1="120" x2="224" y2="120" stroke="#12A594" stroke-opacity="0.6"/><path d="M217,116 L224,120 L217,124" fill="none" stroke="#12A594" stroke-opacity="0.7"/>
<line x1="134" y1="128" x2="224" y2="186" stroke="#12A594" stroke-opacity="0.6"/><path d="M220,180 L224,186 L217,186" fill="none" stroke="#12A594" stroke-opacity="0.7"/>
<text x="178" y="100" font-size="8.5" text-anchor="middle" fill="#12A594" opacity="0.85">#include = paste</text>
<line x1="354" y1="54" x2="446" y2="112" stroke="currentColor" stroke-opacity="0.4"/><path d="M439,109 L446,112 L440,117" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<line x1="354" y1="120" x2="446" y2="120" stroke="currentColor" stroke-opacity="0.4"/><path d="M439,116 L446,120 L439,124" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<line x1="354" y1="186" x2="446" y2="128" stroke="currentColor" stroke-opacity="0.4"/><path d="M440,123 L446,128 L439,131" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<rect x="448" y="92" width="138" height="56" rx="5" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1.2"/>
<text x="517" y="114" font-size="11.5" text-anchor="middle" fill="currentColor">compile + link</text>
<text x="517" y="132" font-size="10.5" text-anchor="middle" fill="#12A594" font-family="var(--md-code-font,monospace)">→ cargoforge</text>
</svg>

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
