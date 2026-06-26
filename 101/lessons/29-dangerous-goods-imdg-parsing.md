# Dangerous-goods (IMDG) parsing

When a ship carries chemicals, explosives, compressed gases, or radioactive
material, ordinary weight-and-balance calculations are not enough. International
maritime law requires every such item to be labelled with a standardised hazard
class and, critically, to be kept a minimum distance away from incompatible
goods. CargoForge-C encodes this information in a compact field on the cargo
manifest line, parses it into a `DGInfo` struct, and later enforces the rules
through a segregation matrix drawn directly from the IMDG Code. This lesson
explains the grammar, the parser, and the physics behind the classes.

---

## What is the IMDG Code?

The **International Maritime Dangerous Goods (IMDG) Code** is the global
rulebook for carrying hazardous cargo by sea. It is published by the
International Maritime Organization (IMO) and has legal force through
SOLAS Chapter VII. Every shipping company, port authority, and ship master
must comply with it.

The Code assigns every dangerous substance a **class** (the broad type of
hazard) and often a **division** (a subdivision of that hazard). It also
assigns a **UN Number** — a four-digit identifier that is universal across all
transport modes.

### The nine IMDG classes

| Class | Name | Example |
|-------|------|---------|
| 1 | Explosives | Ammunition, fireworks |
| 2 | Gases | Propane (2.1), compressed air (2.2), chlorine (2.3) |
| 3 | Flammable liquids | Petrol / gasoline (UN 1203) |
| 4 | Flammable solids / reactive solids | Matches (4.1), white phosphorus (4.2), calcium carbide (4.3) |
| 5 | Oxidizing substances & organic peroxides | Ammonium nitrate (5.1), benzoyl peroxide (5.2) |
| 6 | Toxic and infectious substances | Pesticides (6.1), clinical waste (6.2) |
| 7 | Radioactive material | Uranium ore |
| 8 | Corrosive substances | Sulphuric acid, sodium hydroxide |
| 9 | Miscellaneous dangerous substances | Lithium batteries, dry ice |

Class 1 has six divisions (1.1 through 1.6) representing degrees of explosion
risk, from mass explosion to very insensitive. Classes 2, 4, 5, and 6 each
have two or three divisions. Classes 3, 7, 8, and 9 have no divisions in the
segregation matrix.

---

## The DG field grammar

In a CargoForge-C cargo manifest, every line has four mandatory fields. A
dangerous good adds an optional fifth field using the grammar:

```
DG:<class>[.<division>]:<UNnumber>:<stowage>:<EmS>
```

A concrete example from the digest:

```
FlammableLiquid   25   6x2.5x2.6   hazardous   DG:3.1:UN1203:A:F-E,S-D
```

Breaking that down:

| Token | Value | Meaning |
|-------|-------|---------|
| `DG:` | prefix | Signals this is a dangerous-goods field |
| `3.1` | class 3, division 1 | Flammable liquid, initial boiling point ≤ 35 °C |
| `UN1203` | UN Number | Petrol / gasoline |
| `A` | stowage category | `A` = stow anywhere; `D` = on deck only; `U` = under deck only |
| `F-E,S-D` | EmS reference | Emergency Schedule fire code F-E, spillage code S-D |

!!! note "Division 0"
    If the class has no relevant division (e.g. class 3 or class 8), you may
    write just the integer: `DG:3:UN1203:A:F-E,S-D`. The parser sets
    `dg_division = 0` in that case.

### EmS codes

The Emergency Schedule (EmS) is the ship's guide for fire-fighting and spillage
response. The format is always `F-x,S-y`. The letter after `F-` selects a
fire schedule; the letter after `S-` selects a spillage schedule. These codes
appear in the `DGInfo.ems` field and are preserved exactly as supplied.

---

## Parsing: `parse_dg_field` in `src/parser.c`

The entire DG field is parsed by a single static function. It receives the raw
fifth token from the cargo line and either returns a heap-allocated `DGInfo`
struct, or returns `NULL` if the field is absent, malformed, or carries an
out-of-range class.

```c
/* From src/parser.c:151 */
static DGInfo *parse_dg_field(const char *field) {
    if (!field || strncmp(field, "DG:", 3) != 0)
        return NULL;

    DGInfo *dg = calloc(1, sizeof(DGInfo));
    if (!dg) return NULL;

    /* Work on a copy — strtok_r modifies the string in place */
    char buf[64];
    strncpy(buf, field + 3, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr;
    char *tok = strtok_r(buf, ":", &saveptr);
    if (tok) {
        char *dot = strchr(tok, '.');
        if (dot) {
            *dot = '\0';
            dg->dg_class    = atoi(tok);
            dg->dg_division = atoi(dot + 1);
        } else {
            dg->dg_class    = atoi(tok);
            dg->dg_division = 0;
        }
    }

    tok = strtok_r(NULL, ":", &saveptr);
    if (tok) strncpy(dg->un_number, tok, sizeof(dg->un_number) - 1);

    tok = strtok_r(NULL, ":", &saveptr);
    if (tok) {
        if      (tok[0] == 'D') dg->stowage = STOW_ON_DECK;
        else if (tok[0] == 'U') dg->stowage = STOW_UNDER_DECK;
        else                    dg->stowage = STOW_ANY;
    }

    /* EmS is the remainder — passed as an empty delimiter to consume the rest */
    tok = strtok_r(NULL, "", &saveptr);
    if (tok) strncpy(dg->ems, tok, sizeof(dg->ems) - 1);

    if (dg->dg_class < 1 || dg->dg_class > 9) { free(dg); return NULL; }

    return dg;
}
```

Three design points are worth noting.

**Copy before tokenising.** `strtok_r` writes null bytes into the string as
it splits it. The function copies the field into a 64-byte `buf` so the
original manifest line is never modified.

**`strtok_r` not `strtok`.** The re-entrant variant carries its position in
`saveptr` rather than in hidden global state. This matters because
`parse_cargo_list` is also using `strtok_r` in the same call stack to split
the outer manifest fields; mixing non-re-entrant `strtok` calls would corrupt
both parse states.

**Class validation last.** The function fills all fields first and only then
checks `dg_class < 1 || dg_class > 9`. If the check fails, it frees the
allocation and returns `NULL`. The cargo item is still added to the manifest
— it just has `c->dg = NULL`, so the engine treats it as ordinary hazardous
cargo subject to the legacy 3-metre separation rule rather than IMDG
segregation.

### Where `parse_dg_field` is called

Back in `parse_cargo_list`, after the four mandatory fields are consumed, the
parser reaches for a fifth token:

```c
/* From src/parser.c:319 */
char *dg_field = strtok_r(NULL, " \t\n", &saveptr);

/* ... weight and dimension parsing ... */

if (dg_field) {
    c->dg = (struct DGInfo_ *)parse_dg_field(dg_field);
}
```

The result is stored in `Cargo.dg`. Non-DG cargo has `c->dg = NULL`; DG
cargo has a heap-allocated `DGInfo`. Memory ownership is on the `Cargo` — the
cleanup path in `parse_cargo_list` and `ship_cleanup` both call
`free(ship->cargo[i].dg)` when tearing down the array.

---

## The `DGInfo` struct

The struct (declared in `imdg.h`, allocated in `parser.c`) holds everything
parsed from the DG field:

| Field | Type | Content |
|-------|------|---------|
| `dg_class` | `int` | IMDG class number, 1–9 |
| `dg_division` | `int` | Division number; 0 when unspecified |
| `un_number` | `char[]` | UN number string, e.g. `"UN1203"` |
| `stowage` | `StowageCategory` | `STOW_ANY`, `STOW_ON_DECK`, `STOW_UNDER_DECK` |
| `ems` | `char[]` | EmS reference string, e.g. `"F-E,S-D"` |

The stowage category directly restricts which bin the 3D packer may use.
An item with `STOW_ON_DECK` will trigger an advisory warning if the packer
tries to place it in `ForwardHold` or `AftHold`.

---

## The segregation matrix in `src/imdg.c`

Knowing a cargo's class is not sufficient. The IMDG Code Table 7.2.4 defines
how far apart each pair of classes must be. CargoForge-C encodes this as a
17 × 17 static integer matrix in `src/imdg.c`.

The 17 rows and columns map to the class/division entries listed at the top of
the file:

```
0=1.1-1.6, 1=1.7, 2=1.8, 3=2.1, 4=2.2, 5=2.3,
6=3, 7=4.1, 8=4.2, 9=4.3, 10=5.1, 11=5.2,
12=6.1, 13=6.2, 14=7, 15=8, 16=9
```

Each cell holds an integer encoding the required segregation level:

| Integer | `SegregationType` | Minimum distance |
|---------|-------------------|-----------------|
| 0 | `SEG_NONE` | 0 m (no restriction) |
| 1 | `SEG_AWAY_FROM` | 3 m |
| 2 | `SEG_SEPARATED` | 6 m |
| 3 | `SEG_SEPARATED_COMPLETE` | 12 m |
| 4 | `SEG_SEPARATED_LONG` | 24 m |
| 5 | `SEG_INCOMPATIBLE` | Cannot share the same vessel |

The physical distances come from `imdg_min_distance`:

```c
/* From src/imdg.c:121 */
float imdg_min_distance(SegregationType seg) {
    switch (seg) {
        case SEG_NONE:               return 0.0f;
        case SEG_AWAY_FROM:          return 3.0f;
        case SEG_SEPARATED:          return 6.0f;
        case SEG_SEPARATED_COMPLETE: return 12.0f;
        case SEG_SEPARATED_LONG:     return 24.0f;
        case SEG_INCOMPATIBLE:       return -1.0f;
        default:                     return 0.0f;
    }
}
```

A return value of `-1.0f` for `SEG_INCOMPATIBLE` is a sentinel: the placement
engine treats any negative minimum distance as an absolute hard reject,
regardless of how far apart the items happen to be placed.

Notice the Class 1.1–1.6 row: every cell is `5` (incompatible). Mass
explosives cannot lawfully share a cargo hold with anything else that carries a
DG classification.

### Looking up a pair

`class_to_index` converts a `(dg_class, dg_division)` pair to a matrix index,
then `imdg_get_segregation` does the lookup:

```c
/* From src/imdg.c:108 */
SegregationType imdg_get_segregation(int class_a, int div_a,
                                     int class_b, int div_b) {
    int idx_a = class_to_index(class_a, div_a);
    int idx_b = class_to_index(class_b, div_b);

    if (idx_a < 0 || idx_b < 0 || idx_a >= MATRIX_SIZE || idx_b >= MATRIX_SIZE)
        return SEG_NONE;

    int val = seg_matrix[idx_a][idx_b];
    if (val < 0 || val > 5) return SEG_NONE;
    return (SegregationType)val;
}
```

An unrecognised class returns `SEG_NONE` — a safe, permissive fallback.

---

## Post-placement check: `imdg_check_all`

During 3D bin-packing, `check_cargo_constraints` calls `imdg_get_segregation`
for each candidate position and rejects any placement that would violate
distance or incompatibility rules. But the placement engine works greedily and
may not always achieve the optimal separation. A second, authoritative pass runs
after all cargo is placed.

`imdg_check_all` in `src/imdg.c` performs an O(n²) all-pairs scan of every
placed DG item:

```c
/* From src/imdg.c:165 */
for (int i = 0; i < ship->cargo_count; i++) {
    const Cargo *a = &ship->cargo[i];
    if (a->pos_x < 0 || !a->dg) continue;   /* unplaced or non-DG */

    for (int j = i + 1; j < ship->cargo_count; j++) {
        const Cargo *b = &ship->cargo[j];
        if (b->pos_x < 0 || !b->dg) continue;

        SegregationType required = imdg_get_segregation(
            a->dg->dg_class, a->dg->dg_division,
            b->dg->dg_class, b->dg->dg_division);

        if (required == SEG_NONE) continue;

        float dist = cargo_horizontal_distance(a, b);
        float min_dist = imdg_min_distance(required);
        /* ... record violation if dist < min_dist or incompatible ... */
    }
}
```

Distance is measured edge-to-edge horizontally (the function
`cargo_horizontal_distance` computes centre-to-centre minus half-extents on
each axis, then takes the Euclidean magnitude). Violations are collected in
`IMDGCheckResult.violations[]` up to `MAX_IMDG_VIOLATIONS`.

!!! warning "Items with `dg = NULL` are skipped"
    If `parse_dg_field` returned `NULL` — either because no fifth field was
    present or because the class was out of range — the item is invisible to
    `imdg_check_all`. A cargo line typed as `hazardous` without a DG field
    is still subject to the legacy 3-metre rule applied inside
    `check_hazmat_separation`, but it is exempt from the full segregation
    matrix. Always supply the DG field for real dangerous goods.

---

## End-to-end: a DG cargo item through the system

1. **Manifest line** — the user writes
   `FlammableLiquid 25 6x2.5x2.6 hazardous DG:3.1:UN1203:A:F-E,S-D`.
2. **`parse_cargo_list`** tokenises the line; the fifth token `DG:3.1:UN1203:A:F-E,S-D`
   is passed to `parse_dg_field`.
3. **`parse_dg_field`** allocates a `DGInfo`, sets
   `dg_class=3, dg_division=1, un_number="UN1203", stowage=STOW_ANY, ems="F-E,S-D"`,
   validates the class, and returns the pointer. It is stored in `cargo.dg`.
4. **`find_best_fit_3d`** tries each bin/space/orientation. For each candidate,
   `check_cargo_constraints` calls `imdg_get_segregation(3, 1, ...)` against
   every already-placed DG item and enforces the minimum horizontal distance.
5. **`imdg_check_all`** runs as a final verification after the full manifest is
   placed, reporting any remaining violations.
6. **`ship_cleanup`** calls `free(cargo[i].dg)` for every item, preventing a
   memory leak.

---

## Recap

- The IMDG Code assigns every dangerous substance a **class** (1–9) and often a
  **division**; the UN Number is a universal four-digit identifier.
- CargoForge-C's DG field grammar is
  `DG:<class>[.<division>]:<UNnumber>:<stowage>:<EmS>`,
  parsed by `parse_dg_field` in `src/parser.c` into a heap-allocated `DGInfo`.
- `strtok_r` (re-entrant) is used instead of `strtok` because the outer cargo
  line tokeniser is active in the same call stack.
- The segregation matrix in `src/imdg.c` is a 17 × 17 integer table encoding
  five levels of required separation, from "away from" (3 m) to
  "incompatible" (hard reject).
- A first separation check runs during bin-packing; `imdg_check_all` performs
  a definitive O(n²) post-placement audit.
- Items without a DG field have `cargo.dg = NULL` and are handled by the
  simpler legacy hazmat distance rule, not the full IMDG matrix.

*Next: [Lab 7 — Break the parser (safely)](lab-07-parsing.md).*
