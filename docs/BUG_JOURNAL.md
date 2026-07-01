# Bug Journal

## Patterns to scan for FIRST

- **`free(ptr)` not immediately followed by `ptr = NULL`** on any error path that
  a cleanup function might later iterate over. If a count/length field tracked
  separately from the pointer isn't also zeroed at the same time, a later
  guard like `if (ptr)` or a loop bound by the stale count will walk freed
  memory. Grep for `free(` and check the next two lines by hand.
- **Two-pass parsers that allocate before validating every field**: any
  `malloc`/`calloc` that happens before all input is validated needs its
  error path audited for exactly the pattern above.

## Chronological log (newest first)

- `src/parser.c:336`, `src/parser.c:364` — Symptom: `ship_cleanup` read freed
  memory (heap-use-after-free, ASan exit 134) when a manifest had a bad weight
  or dimension field partway through. Cause: the error path in
  `parse_cargo_list` called `free(ship->cargo)` but left `ship->cargo` and
  `ship->cargo_count` unchanged, so `ship_cleanup`'s loop later walked the
  freed array. Fix: set `ship->cargo = NULL; ship->cargo_count = 0;`
  immediately after `free`, in both error paths. Lesson: an allocator's `free`
  never touches the pointer variable itself — the caller must invalidate it.
  Found by `scripts/fuzz.sh` under AddressSanitizer; see lessons 13, 39, 40
  and labs 1-3 in `101/` for the full walkthrough.
