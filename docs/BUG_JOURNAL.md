# CargoForge-C — Bug Journal

Cheap to write, cheap to read, expensive to skip. Grep this **before** debugging a
new symptom. Newest entries first.

---

## Patterns to scan for FIRST

- **Uninitialized struct fields used as sentinels.** Any `malloc`'d array of
  structs whose code later tests a field (`pos_x >= 0`, `count != -1`) to infer
  state. `malloc` does not zero — use `calloc` or set the sentinel explicitly
  right after allocation. Reads of uninitialized memory are UB and silently
  corrupt downstream math (here: the CG optimizer).
- **Geometry placed without a bounds check on every axis.** A bin/shelf packer
  must verify the item fits in *every* dimension before committing. Check the
  "open a new shelf/row" branch too — that's where the X-extent check was missing.
- **Two functions that independently re-derive the same decision.** `find_spot`
  computed a placement, `commit` re-computed it. They drifted. Make one function
  the single source of truth and have the other *apply* its result.
- **Required config keys not validated as present.** A missing `width_m` left
  `ship->width == 0` and divided by zero in the GM math. After parsing, assert
  every required field was actually set, not just that each line that *was*
  present parsed correctly.
- **Tests asserting brittle exact coordinates.** A heuristic's output changes
  when the heuristic improves. Assert invariants (in-bounds, no overlap, all
  placed, CG within tolerance), not magic numbers.

---

## Chronological log (newest first)

### 2026-06-25 — JSON ship config bypassed the numeric validation the text path enforces
- **Symptom:** `{"holds": 1e30}` → UB on the `(int)v` cast; `{"length_m": -5}` or NaN
  flowed unchecked into the stability math (the `length < 0.1f` guard is false for NaN).
- **Cause:** `parse_ship_config_json` read fields with `json_number` only; the text path
  validates every field via `safe_atof` (finite + range). Found by the Opus parser audit.
- **Fix:** `json_field()` helper validates finiteness + [0.1, 1e9] (holds 1..MAX_HOLDS)
  for every JSON numeric field; non-finite/out-of-range now aborts. parser.c.
- **Lesson:** a second parser for the same data must reuse the SAME validation. UBSan
  catches the `(int)` cast of a non-finite float — run it on the new path.

### 2026-06-25 — Stacking enforced only per-item weight, not cumulative column load
- **Symptom:** a floor crate rated `maxstack=8` accepted an unbounded pile of 5 t items
  (each ≤ 8 individually); cumulative load was never checked.
- **Cause:** `stack_cargo` checked `item->weight` vs the immediate base only. placement_2d.c.
- **Fix:** track per-column cumulative load keyed by the floor index; reject when
  `col_load[floor] + item > floor.max_stack_t`. Plus an `isfinite` guard on stack height.
- **Lesson:** "max stack weight" is a column constraint, not a point constraint. Test
  the cumulative case (regression test asserts 1 stacks, not 4).

### 2026-06-25 — Cargo `temp=`/`maxstack=` used unvalidated strtof
- **Symptom:** `maxstack=abc` silently became 0.0 → that base forbids ALL stacking;
  `temp=abc` became 0.0 °C while still flagging the item reefer.
- **Cause:** `strtof(a+N, NULL)` ignored conversion failure. parser.c.
- **Fix:** check the end pointer + finiteness (+ non-negative for maxstack); warn & skip.
- **Lesson:** never `strtof(..., NULL)` on user input — always inspect `endptr`.


### 2026-06-23 — Transverse CG skewed (load piled against one side of the beam)
- **Symptom:** sample scenario reported `Balance: Warning` (transverse CG 31%); the
  shelf packer stacks rows from y=0 upward, so mass sat against one side.
- **Cause:** within-bin Y placement always grows from the bin's y_offset; the
  CG-aware *bin* selection can't move an item within a bin.
- **Fix:** a post-placement "transverse trim" in `place_cargo_2d` shifts each bin's
  whole loaded band to center it on the beam (a uniform per-bin Y-shift). Sample
  went 31% -> 49% (`Good`). NOTE for future debuggers: `pos_y` therefore does NOT
  equal the raw shelf offset — it is the shelf offset plus the per-bin centering shift.
- **Lesson:** a cheap post-process that respects invariants (uniform shift can't
  overlap or exceed bounds) beat rewriting the packer or splitting bins (which would
  have made beam-spanning cargo unplaceable).

### 2026-06-23 — Double-free when cargo parse fails after allocation
- **Symptom:** ASan abort "attempting double-free" on a manifest with a bad
  weight/dimension; `main.c:30` frees a pointer the parser already freed.
- **Cause:** `parse_cargo_list` `free(ship->cargo)` on its error paths but left the
  field dangling; `main` frees `ship.cargo` again on any parse failure.
- **Fix:** set `ship->cargo = NULL` after the parser's internal frees (parser.c).
- **Lesson:** after `free`, NULL the pointer if anyone else can reach it —
  `free(NULL)` is safe, freeing a dangling pointer is not. Found only because the
  sanitizer CI/`make sanitize` exercises the error paths.

### 2026-06-23 — Parser left position fields uninitialized → UB in CG optimizer
- **Symptom:** placement output subtly non-deterministic / influenced by heap garbage.
- **Cause:** `parser.c` `parse_cargo_list` used `malloc` and never set `pos_x/y/z`;
  `placement_2d.c` reads `pos_x >= 0` to mean "already placed."
- **Fix:** initialize `pos_x = pos_y = pos_z = -1`, `placed_w = placed_h = 0` per item.
- **Lesson:** `malloc` ≠ zeroed. Sentinel fields must be set explicitly.

### 2026-06-23 — Placement could exceed bin X-extent (out-of-bounds stow)
- **Symptom:** an item longer than a hold's length would still be "placed."
- **Cause:** `find_spot_in_bin` new-shelf branch checked Y-extent + shelf count but
  not `cw <= bin->w`; `commit_placement_to_bin` duplicated the gap.
- **Fix:** single-source `find_placement()` returns a full decision (shelf index or
  new-shelf) with an X-extent check on both branches; `commit` applies it verbatim.
- **Lesson:** check every axis; don't let two functions re-derive one decision.

### 2026-06-23 — Missing ship-config field divided by zero in GM math
- **Symptom:** GM/draft math could divide by `ship->width == 0`.
- **Cause:** parser validated each present line but never checked required keys exist.
- **Fix:** post-parse validation that `length`, `width`, `max_weight` were all set.
- **Lesson:** "every line parsed" ≠ "every required field present."

### 2026-06-23 — Stale placement test asserted pre-refactor coordinates
- **Symptom:** `make test` aborted on `test_placement_2d` line 49.
- **Cause:** test hard-coded "HeavyBox → Hold1, pos_x < 10"; the CG-aware refactor
  now places the first heavy item near the ship's center (Hold2) — which is *more*
  correct for stability.
- **Fix:** rewrote the test to assert invariants (in-bounds, no overlap, all placed).
- **Lesson:** test heuristics by their invariants, not by frozen output.
