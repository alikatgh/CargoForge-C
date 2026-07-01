# Dangerous-goods (IMDG) parsing

When a ship carries chemicals, explosives, compressed gases, or radioactive
material, ordinary weight-and-balance calculations are not enough. International
maritime law requires every such item to be labelled with a standardised hazard
class and, critically, to be kept a minimum distance away from incompatible
goods. CargoForge-C encodes this information in a compact field on the cargo
manifest line, parses it into a `DGInfo` struct, and later enforces the rules
through a segregation matrix drawn directly from the IMDG Code. This lesson
explains the grammar, the parser, and the physics behind the classes.

## The mental model 🧠

A dangerous-goods tag is a lot of safety law compressed into one field. Most cargo is inert and carries nothing extra — but a drum of flammable liquid rides with a compact code, `DG:3.1:UN1203:A:F-E`, that names its hazard class, its UN number, and its handling rules. CargoForge parses that string into a `DGInfo` struct that hangs off the cargo item by a pointer, so ordinary cargo pays nothing for fields it does not need (its `dg` is simply `NULL`).

The class is not decoration — it is the key into a **segregation matrix** lifted straight from the IMDG Code, the table that says which hazards may not ride near which (an oxidiser must stay clear of a flammable). Parsing the tag is step one; Lesson 33 enforces the distances. The grammar is strict on purpose: a mis-parsed hazard class is not a cosmetic bug, it is two incompatible chemicals stowed side by side.

<svg viewBox="0 0 600 220" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:560px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>Parsing a dangerous-goods tag into a DGInfo struct</title>
<desc>A manifest line for hazardous cargo ends in a compact DG tag naming the hazard class and UN number. parse_dg_field turns it into a DGInfo struct that hangs off the cargo item by a pointer; ordinary cargo leaves that pointer NULL. The hazard class feeds the segregation matrix that keeps incompatible goods apart.</desc>
<rect x="18" y="18" width="564" height="30" rx="4" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.35"/>
<text x="28" y="37" font-size="10.5" fill="currentColor" opacity="0.7" font-family="var(--md-code-font,monospace)">FlammableLiquid 25 6x2.5x2.6 hazardous <tspan fill="#12A594" font-weight="600">DG:3.1:UN1203:A:F-E</tspan></text>
<line x1="333" y1="49" x2="412" y2="76" stroke="#12A594" stroke-opacity="0.7"/><path d="M405,73 L413,77 L406,80" fill="none" stroke="#12A594"/>
<rect x="356" y="78" width="160" height="30" rx="5" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.1"/><text x="436" y="98" font-size="10.5" text-anchor="middle" fill="currentColor" font-family="var(--md-code-font,monospace)">parse_dg_field()</text>
<line x1="436" y1="108" x2="436" y2="126" stroke="currentColor" stroke-opacity="0.45"/><path d="M432,119 L436,127 L440,119" fill="none" stroke="currentColor" stroke-opacity="0.5"/>
<rect x="350" y="128" width="172" height="74" rx="5" fill="currentColor" fill-opacity="0.04" stroke="currentColor" stroke-opacity="0.45"/>
<text x="362" y="146" font-size="10.5" fill="currentColor" opacity="0.85" font-family="var(--md-code-font,monospace)">DGInfo</text>
<text x="362" y="164" font-size="9.5" fill="currentColor" opacity="0.7">class 3.1 — flammable</text>
<text x="362" y="180" font-size="9.5" fill="currentColor" opacity="0.7">UN 1203</text>
<text x="362" y="196" font-size="9.5" fill="currentColor" opacity="0.7">segregation: F-E, S-D</text>
<line x1="350" y1="165" x2="246" y2="165" stroke="#D05663" stroke-opacity="0.6"/><path d="M253,161 L245,165 L253,169" fill="none" stroke="#D05663" stroke-opacity="0.7"/>
<rect x="60" y="146" width="184" height="40" rx="5" fill="#D05663" fill-opacity="0.07" stroke="#D05663" stroke-opacity="0.55"/>
<text x="152" y="164" font-size="10" text-anchor="middle" fill="currentColor">segregation matrix</text>
<text x="152" y="178" font-size="8.5" text-anchor="middle" fill="#D05663" opacity="0.9">keeps incompatibles apart · Lesson 33</text>
<text x="28" y="92" font-size="9.5" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">ordinary cargo:</text>
<text x="28" y="108" font-size="9.5" fill="currentColor" opacity="0.6" font-family="var(--md-code-font,monospace)">dg = NULL</text>
<text x="28" y="123" font-size="8.5" fill="currentColor" opacity="0.45">(no DGInfo allocated)</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **IMDG class** = "a number from 1 to 9 that tells you what kind of danger a cargo item poses" — CargoForge-C reads this as `dg_class` inside the `DGInfo` struct; without it, the engine has no idea whether a crate contains explosives or just a flammable liquid.
- **UN Number** = "a four-digit universal code that identifies the exact substance, the same on every ship, truck, and train in the world" — stored in `DGInfo.un_number` (e.g. `"UN1203"` for petrol); it is parsed verbatim from the fifth field of the manifest line by `parse_dg_field`.
- **`strtok_r` instead of `strtok`** = "a thread-safe, re-entrant version of the standard string-splitter that keeps its own position variable" — `parse_dg_field` is called from inside `parse_cargo_list`, which is itself mid-split using `strtok_r`; mixing in plain `strtok` would corrupt both parse states simultaneously.
- **Segregation matrix** = "a 17 × 17 lookup table that says how many metres two dangerous-goods classes must be kept apart" — `imdg_get_segregation` reads this table by converting each `(dg_class, dg_division)` pair to a row/column index; an entry of `5` (`SEG_INCOMPATIBLE`) means the two items cannot share the same vessel at all.
- **`cargo.dg = NULL`** = "this item has no IMDG record and is invisible to the full segregation check" — if `parse_dg_field` cannot recognise the class or the fifth field is absent, it frees its allocation and returns `NULL`; `imdg_check_all` then skips that item entirely and only the coarser legacy 3-metre hazmat rule applies.
- **`imdg_check_all`** = "a final all-pairs scan that confirms every placed DG item is far enough from every other one" — it runs after `find_best_fit_3d` has already packed all cargo, catching any distance violations the greedy packer could not avoid.

**Why it matters:** if a dangerous-goods field is missing or malformed, the item silently falls through to the weaker legacy rule, and a real incompatibility — say, Class 1 explosives next to Class 5 oxidisers — would never be flagged; on a real vessel that is a SOLAS violation and a potential catastrophe.

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

## Parsing: `parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)

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

## The segregation matrix in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c)

Knowing a cargo's class is not sufficient. The IMDG Code Table 7.2.4 defines
how far apart each pair of classes must be. CargoForge-C encodes this as a
17 × 17 static integer matrix in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c).

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

`imdg_check_all` in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c) performs an O(n²) all-pairs scan of every
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
  parsed by `parse_dg_field` in [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c) into a heap-allocated `DGInfo`.
- `strtok_r` (re-entrant) is used instead of `strtok` because the outer cargo
  line tokeniser is active in the same call stack.
- The segregation matrix in [`src/imdg.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/imdg.c) is a 17 × 17 integer table encoding
  five levels of required separation, from "away from" (3 m) to
  "incompatible" (hard reject).
- A first separation check runs during bin-packing; `imdg_check_all` performs
  a definitive O(n²) post-placement audit.
- Items without a DG field have `cargo.dg = NULL` and are handled by the
  simpler legacy hazmat distance rule, not the full IMDG matrix.

## Check yourself

??? question "Why does ordinary, non-hazardous cargo pay zero memory cost for dangerous-goods fields it doesn't need?"
    A Cargo item's `dg` pointer is simply `NULL` for ordinary cargo. A `DGInfo` struct is only heap-allocated when the manifest line actually carries a `DG:` tag, so plain cargo never pays for fields it will never use.

??? question "What does the IMDG segregation matrix actually decide?"
    The minimum required physical distance — or outright incompatibility — between two different hazard classes, so that, say, an oxidiser and a flammable liquid aren't stowed close enough together to interact dangerously.

*Next: [Lab 7 — Break the parser (safely)](lab-07-parsing.md).*
