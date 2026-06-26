# The whole pipeline, end to end

Every earlier lesson has focused on one layer: parsing, physics, placement, or analysis. This lesson joins them by tracing a single real invocation — `cargoforge optimize ship.cfg cargo.txt` — through every function that executes, from the first byte read off disk to the last stability number printed on screen. Follow this trace and the overall architecture becomes concrete.

<svg viewBox="0 0 660 180" role="img" xmlns="http://www.w3.org/2000/svg" style="max-width:640px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
<title>The CargoForge-C pipeline: parse, place, analyze, report</title>
<desc>main and the CLI dispatch one run. The parser fills a Ship struct from the input files; placement assigns each cargo a 3D position; analysis computes stability; the output stage prints the report. The same Ship struct flows through every stage.</desc>
<defs><marker id="pp-ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 1 L9 5 L0 9 Z" fill="#12A594"/></marker></defs>
<text x="330" y="32" fill="currentColor" font-size="11.5" text-anchor="middle" opacity="0.7"><tspan font-family="var(--md-code-font,monospace)">cargoforge optimize ship cargo</tspan>  →  main.c / cli.c</text>
<rect x="20" y="56" width="142" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.75"/>
<text x="91" y="80" fill="currentColor" font-size="12" text-anchor="middle">parse</text>
<text x="91" y="98" fill="currentColor" font-size="9.5" text-anchor="middle" opacity="0.6" font-family="var(--md-code-font,monospace)">parser.c</text>
<rect x="186" y="56" width="142" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.75"/>
<text x="257" y="80" fill="currentColor" font-size="12" text-anchor="middle">place</text>
<text x="257" y="98" fill="currentColor" font-size="9.5" text-anchor="middle" opacity="0.6" font-family="var(--md-code-font,monospace)">placement_3d.c</text>
<rect x="352" y="56" width="142" height="56" rx="6" fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.75"/>
<text x="423" y="80" fill="currentColor" font-size="12" text-anchor="middle">analyze</text>
<text x="423" y="98" fill="currentColor" font-size="9.5" text-anchor="middle" opacity="0.6" font-family="var(--md-code-font,monospace)">analysis.c</text>
<rect x="518" y="56" width="122" height="56" rx="6" fill="#12A594" fill-opacity="0.1" stroke="#12A594" stroke-width="1.4"/>
<text x="579" y="80" fill="#12A594" font-size="12" text-anchor="middle">report</text>
<text x="579" y="98" fill="#12A594" font-size="9.5" text-anchor="middle" opacity="0.75" font-family="var(--md-code-font,monospace)">json_output.c</text>
<line x1="162" y1="84" x2="184" y2="84" stroke="#12A594" stroke-width="1.4" marker-end="url(#pp-ar)"/>
<line x1="328" y1="84" x2="350" y2="84" stroke="#12A594" stroke-width="1.4" marker-end="url(#pp-ar)"/>
<line x1="494" y1="84" x2="516" y2="84" stroke="#12A594" stroke-width="1.4" marker-end="url(#pp-ar)"/>
<text x="330" y="150" fill="currentColor" font-size="11.5" text-anchor="middle" opacity="0.65">One <tspan font-family="var(--md-code-font,monospace)">Ship</tspan> struct flows through every stage — each fills in more of it.</text>
</svg>

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **The pipeline** = "one command triggers five stages in sequence, each handing its results to the next" — `cmd_optimize` in `cli.c` is the single function that owns this chain: it calls `parse_ship_config`, `parse_cargo_list`, `place_cargo_3d`, `perform_analysis`, `output_results`, and `ship_cleanup` in exactly that order, with one `Ship` struct flowing through all of them.
- **`Ship` struct as the shared state** = "a single data structure that every stage reads from and writes into" — the parser fills in dimensions and the cargo array, `place_cargo_3d` sets each cargo's `pos_x/y/z`, and `perform_analysis` reads those positions to compute GM; if any stage leaves the struct half-formed, every later stage gets wrong answers.
- **Sentinel values (`pos_x = -1.0f`, `gm = NAN`)** = "a special number baked into the data itself that means 'something went wrong here'" — instead of threading a separate error flag through every function, `perform_analysis` skips any cargo item whose `pos_x` is still `-1.0f` (never placed), and every output formatter checks `isnan(result.gm)` before printing stability numbers.
- **`safe_atof` and fail-fast validation** = "stop the moment any input number is bad, rather than carrying garbage silently forward" — if a weight or dimension string in `parse_cargo_list` is out of range, the parser immediately returns `-1`, and `cmd_optimize` aborts before any placement or physics runs, preventing the heap-use-after-free bug that would follow if a half-built cargo array were freed twice.
- **Guillotine bin-packing (`split_space_3d`, `find_best_fit_3d`)** = "cut the remaining free space into up to three smaller rectangles every time a box is placed" — the three bins (`ForwardHold`, `AftHold`, `Deck`) each start as one big empty space; every placed cargo slices it into right, back, and top remainders, and `find_best_fit_3d` picks whichever valid slot wastes the least volume.
- **`ship_cleanup` and NULL-after-free** = "free every heap allocation in reverse order, then set the pointer to NULL so a second call is harmless" — the parser allocates `ship->cargo`, each `cargo[i].dg`, `ship->hydro`, `ship->tanks`, and `ship->strength_limits`; cleanup frees them in the same strict sequence and nulls each pointer so a double-call cannot dereference freed memory.

**Why it matters:** if any stage produces bad data — an undetected NAN, a non-nulled pointer after an early-exit free, or a skipped cargo item — every downstream stage silently inherits the corruption; the whole-pipeline view in this lesson is what lets you trace a wrong output number back to the exact stage and line that produced it.

---

## The mental model 🧠

You'll forget the call graph — hold THIS picture instead:

> A shipping container is loaded onto a truck at the factory, transferred to a train, then to a port, then craned into a ship's hold. At every handoff, the same physical container travels — workers just move it and update the manifest. If it gets damaged at the train yard, every stop after that inherits the problem.

The `Ship` struct is that container. `parse_ship_config` and `parse_cargo_list` pack it at the factory (filling dimensions, cargo weight, position sentinels). `place_cargo_3d` moves it to the train — setting each cargo's real `pos_x/y/z` and replacing the `-1.0f` sentinels. `perform_analysis` cranes it aboard and reads those positions to compute GM and the GZ curve. `output_results` delivers the manifest.

The sentinel values (`pos_x = -1.0f`, `gm = NAN`) are the damage stickers on the container: every downstream stop checks for them before doing work. `ship_cleanup` is the port authority clearing the manifest at the end — it frees memory in reverse-allocation order and NULLs every pointer so a second check finds nothing to clear.

---

## Entry: `main.c`

The program starts in [`src/main.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/main.c), which is deliberately thin:

```c
/* src/main.c */
int main(int argc, char *argv[]) {
    CLIContext ctx;
    init_cli_context(&ctx);

    int result = parse_cli_args(argc, argv, &ctx);
    if (result <= 0) {
        free_cli_context(&ctx);
        return (result == 0) ? EXIT_SUCCESS : EXIT_INVALID_ARGS;
    }

    int exit_code = dispatch_subcommand(&ctx);
    free_cli_context(&ctx);
    return exit_code;
}
```

Three function calls: initialise a context struct, parse the command line into it, dispatch to the right subcommand. Nothing else. All the real work lives in [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c) and the modules it calls.

`CLIContext` carries the parsed arguments — `ship_file`, `cargo_file`, `format`, `verbose`, `show_viz`, and similar flags. Its fields start as zero/false and are populated by `parse_cli_args`.

---

## Stage 1 — Argument parsing (`cli.c`)

`init_cli_context` sets default values (`FORMAT_HUMAN`, `show_viz = true`, `color = isatty(STDERR_FILENO)`) and reads two optional config files: `~/.cargoforgerc` and a project-local `.cargoforgerc`. Either file can override the format, color, and verbosity defaults without touching the command line.

`parse_cli_args` (from [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c)) uses POSIX `getopt_long` to process flags such as `--format=json` and `--no-viz`. Positional arguments after the flags are collected into `ctx->ship_file` and `ctx->cargo_file`:

```c
/* src/cli.c */
if (optind < argc) ctx->ship_file = argv[optind++];
if (optind < argc) ctx->cargo_file = argv[optind++];
```

`dispatch_subcommand` then reads `ctx->subcommand` (the string `"optimize"`) and calls `cmd_optimize(ctx)`.

---

## Stage 2 — Parsing (`parser.c`)

`cmd_optimize` in [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c) owns the top-level pipeline. The first thing it does is parse:

```c
/* src/cli.c — cmd_optimize */
Ship ship = {0};

if (parse_ship_config(ctx->ship_file, &ship) != 0) { ... return EXIT_PARSE_ERROR; }
if (parse_cargo_list(ctx->cargo_file, &ship) != 0) { ship_cleanup(&ship); return EXIT_PARSE_ERROR; }
```

The `Ship` struct is stack-allocated and zero-initialised. The two parse functions populate it from disk.

### `parse_ship_config`

`parse_ship_config` (from [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)) opens the file, then walks it line by line with `fgets`. Lines starting with `#` or empty lines are skipped. Every other line is split on `=` with `strtok` to produce a key and a value.

String-valued keys (`hydrostatic_table`, `tank_config`) are stashed in local path buffers; numeric keys are passed through `safe_atof`:

```c
/* src/parser.c */
static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0;
    float val = strtof(s, &end);
    if (errno != 0 || end == s || (*end != '\0' && *end != '\n') || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n", field_name, s);
        return NAN;
    }
    return val;
}
```

If `safe_atof` returns `NAN`, the parser returns `-1` immediately and parsing aborts. This is the codebase's uniform strategy for input validation: make the error visible and stop early, never silently carry garbage forward.

After the key=value loop closes, the parser loads optional sub-tables. If `hydrostatic_table` was set, it calls `parse_hydro_table` and stores the result in a heap-allocated `HydroTable` at `ship->hydro`. If `tank_config` was set, it calls `parse_tank_config` and stores the result in `ship->tanks`. If any permissible-strength keys appeared, a `StrengthLimits` struct is heap-allocated and assigned to `ship->strength_limits`.

When the function returns `0`, `ship` holds every dimensional and mass property the rest of the pipeline needs, plus pointers to optional hydrostatic, tank, and strength data.

### `parse_cargo_list`

`parse_cargo_list` (from [`src/parser.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/parser.c)) needs to allocate the `ship->cargo` array before it can populate it. For files (the common case) it does two passes: count data lines on the first pass, then `rewind` and parse on the second. For stdin it buffers all lines in memory first.

Each non-comment line is split into mandatory fields with `strtok_r`:

```c
/* src/parser.c */
char *id     = strtok_r(line, " \t",   &saveptr);
char *w_str  = strtok_r(NULL, " \t",   &saveptr);
char *dim_str= strtok_r(NULL, " \t",   &saveptr);
char *type   = strtok_r(NULL, " \t\n", &saveptr);
/* optional 5th field */
char *dg_field = strtok_r(NULL, " \t\n", &saveptr);
```

The weight string goes through `safe_atof` (range 0.1–10⁶ tonnes) and is multiplied by 1000 to convert to kilograms. Dimensions are split on `x` and parsed the same way. The cargo's initial position is set to `-1.0f` on all three axes — the sentinel that means "not yet placed":

```c
c->pos_x = -1.0f;
c->pos_y = -1.0f;
c->pos_z = -1.0f;
```

If the optional DG field is present, `parse_dg_field` parses the grammar `DG:class.division:UNnumber:stowage:EmS` and returns a heap-allocated `DGInfo` struct assigned to `c->dg`. Non-DG cargo leaves `c->dg = NULL`.

!!! note "The dangling-pointer fix"
    If weight or dimension parsing fails mid-list, the error path must free the partially-built `ship->cargo` array. The current code immediately sets `ship->cargo = NULL` and `ship->cargo_count = 0` after calling `free`. Without that NULL-out, `ship_cleanup` would later iterate the stale count and dereference freed memory — the heap-use-after-free bug described in the digest (section 7). The comment in the source says: `// avoid a dangling pointer -> use-after-free/double-free in ship_cleanup`.

---

## Stage 3 — 3D bin-packing (`placement_3d.c`)

With both the ship and cargo fully in memory, `cmd_optimize` calls:

```c
/* src/cli.c — cmd_optimize */
place_cargo_3d(&ship);
```

`place_cargo_3d` (from [`src/placement_3d.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/placement_3d.c)) starts by sorting the cargo array in descending volume order — the First Fit Decreasing (FFD) heuristic, which places large items first so they do not get crowded out later:

```c
/* src/placement_3d.c */
qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp_by_volume_desc);
```

Next it creates three hard-coded `Bin3D` structs on the stack:

| Bin | X origin | Width | Max weight |
|---|---|---|---|
| `ForwardHold` | 0 | L × 0.30 | max\_weight × 0.30 |
| `AftHold` | L × 0.70 | L × 0.30 | max\_weight × 0.30 |
| `Deck` | 0 | L | max\_weight × 0.40 |

The holds sit at `z = -8.0 m` (below waterline, 8 m tall, 80% of beam wide). The deck sits at `z = 0` (4 m tall, full beam). Each bin starts with one free `Space3D` covering its entire interior.

For each cargo item, the algorithm calls `find_best_fit_3d`. This function iterates every bin × every free space × all six axis-aligned orientations of the item. For each combination it checks two things in order: the bin's remaining weight capacity, then the per-item constraints via `check_cargo_constraints` (point load, IMDG separation, stacking pressure, reefer preference, deck ratio). Among all valid placements it picks the one with the smallest space volume — the tightest fit, minimising wasted headroom.

When a placement is found, `split_space_3d` performs a guillotine split: the chosen space is marked occupied and up to three new free spaces are carved from the leftovers (right remainder, back remainder, top remainder). The cargo item's position fields are set to the corner of the space it occupies:

```c
/* src/placement_3d.c */
c->pos_x = space->x;
c->pos_y = space->y;
c->pos_z = space->z;
bin->current_weight += c->weight;
split_space_3d(bin, best_space, c, best_orientation);
```

Items that cannot be placed (no valid space in any bin) keep their sentinel coordinates (`pos_x = -1.0f`) and a warning is printed to stderr. After all items are processed, a placement summary goes to stderr showing how full each bin is.

---

## Stage 4 — Stability analysis (`analysis.c`)

Back in `cmd_optimize`, the next call is:

```c
/* src/cli.c — cmd_optimize */
AnalysisResult result = perform_analysis(&ship);
```

`perform_analysis` (from [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)) returns an `AnalysisResult` by value. It runs in one logical sequence.

**Cargo accumulation.** A single loop over `ship->cargo` skips any item with `pos_x < 0` (unplaced). For placed items it accumulates the total weight, transverse moment, longitudinal moment, and vertical moment. The centre of gravity of each item is taken at the geometric centre of its bounding box, for example the vertical position is `pos_z + height / 2`.

**Overweight rejection.** If `lightship_weight + cargo_weight + tank_weight > ship->max_weight`, the function sets `r.gm = NAN` and returns immediately. The NAN propagates into every downstream consumer as a signal that the plan is infeasible.

**Hydrostatics.** If `ship->hydro->loaded` is true, `hydro_draft_from_displacement` inverse-interpolates the draft from the CSV table, then `hydro_interpolate` reads KB, BM, and MTC at that draft. If there is no hydrostatic table, the box-hull fallback applies:

$$\text{draft} = \frac{V_\text{displaced}}{L \times B \times C_b}$$

where $C_b = 0.75$, $K_B = 0.53 \times \text{draft}$, and

$$BM = \frac{L \times B^3}{12} \times C_w \div V_\text{displaced}$$

with $C_w = 0.85$.

**GM and free-surface correction.** With KB and BM in hand:

$$GM = KB + BM - KG$$

If tank data is loaded, `calculate_virtual_kg_rise` computes the free-surface correction (FSC) — the virtual rise in KG caused by liquid sloshing in partially-filled tanks. The corrected metacentric height used for all subsequent criteria is:

$$GM_\text{corrected} = GM - \text{FSC}$$

**Trim and heel.** Longitudinal centre of gravity (LCG) is the cargo moment about midship divided by displacement. Trim by stern is:

$$\text{trim} = LCG \times L \div GM_L$$

Transverse centre of gravity (TCG) offset from centreline drives heel:

$$\text{heel} = \arctan\!\left(\frac{TCG}{GM_\text{corrected}}\right) \times \frac{180}{\pi}$$

**GZ curve and IMO criteria.** All GZ values use the wall-sided formula:

$$GZ(\theta) = \sin\theta \times \left[GM_\text{corrected} + BM \times \frac{\tan^2\theta}{2}\right]$$

`gz_at_angle` evaluates this at 30°. `find_gz_max` scans 1°–80° in 1° steps. `integrate_gz` applies the trapezoidal rule over 100 steps to compute the three IMO areas. All six thresholds are checked in a single boolean expression; `imo_compliant` is set to 1 only if all six pass.

**Longitudinal strength.** If `ship->strength_limits` is non-NULL, `calculate_longitudinal_strength` distributes lightship weight, cargo, and buoyancy across 20 hull stations, integrates to shear force and then to bending moment, and `check_strength_limits` compares the peaks against the class-society limits.

---

## Stage 5 — Output (`cli.c`)

The final call in `cmd_optimize` is:

```c
/* src/cli.c — cmd_optimize */
output_results(&ship, &result, ctx->format, ctx->output_file);
```

`output_results` (from [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c)) switches on format:

- `FORMAT_HUMAN` — calls `print_loading_plan` (which re-runs `perform_analysis` internally and prints a structured human-readable report) followed by `print_cargo_layout_ascii` and `print_cargo_summary` if `show_viz` is true.
- `FORMAT_JSON` — calls `print_json_output` from `json_output.c`, which serialises the full `Ship` and `AnalysisResult` to JSON. Fields that would be meaningless after an overweight rejection (GM is NAN) are emitted as `null`.
- `FORMAT_CSV`, `FORMAT_TABLE`, `FORMAT_MARKDOWN` — `output_csv`, `output_table`, `output_markdown` write tabular cargo placement data plus a summary line.

If `--output=FILE` was given, output goes to that file; otherwise it goes to stdout.

---

## Stage 6 — Cleanup

After output, the last act of `cmd_optimize` is:

```c
/* src/cli.c — cmd_optimize */
ship_cleanup(&ship);
return EXIT_SUCCESS;
```

`ship_cleanup` (from [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)) frees every heap allocation that was made during parsing: first it iterates the cargo array and frees each `cargo[i].dg`, then it frees the cargo array itself, then `ship->hydro`, `ship->tanks`, and `ship->strength_limits`. It NULLs every pointer after freeing so that a double-call is safe.

---

## The call graph at a glance

```
main()
  init_cli_context()        — defaults + ~/.cargoforgerc
  parse_cli_args()          — getopt_long → ctx
  dispatch_subcommand()
    cmd_optimize()
      parse_ship_config()   — key=value → Ship fields + hydro/tanks/limits
        parse_hydro_table() — optional CSV → ship->hydro
        parse_tank_config() — optional CSV → ship->tanks
      parse_cargo_list()    — manifest → ship->cargo[0..n]
        parse_dg_field()    — optional DG: field → cargo[i].dg
      place_cargo_3d()      — FFD sort → 3 bins → pos_x/y/z set
        find_best_fit_3d()
          check_cargo_constraints()
        split_space_3d()
      perform_analysis()    — physics → AnalysisResult
        hydro_draft_from_displacement() / box-hull
        calculate_virtual_kg_rise()
        gz_at_angle() / integrate_gz() / find_gz_max()
        calculate_longitudinal_strength()
      output_results()      — human / JSON / CSV / table / markdown
      ship_cleanup()        — free cargo[i].dg, cargo, hydro, tanks, limits
```

!!! tip "Sentinel values as pipeline signals"
    Notice that unplaced cargo uses `pos_x = -1.0f` and an overweight plan uses `gm = NAN`. Both are in-band signals: downstream code checks them with simple comparisons (`c->pos_x < 0`, `isnan(a.gm)`) instead of threading an error code through every function. This is a deliberate design choice — one value carries both data and status, at the cost of needing to check it in every consumer.

---

## Recap

- `main.c` is 12 lines; it initialises context, parses args, and dispatches. All logic is in the modules.
- `cmd_optimize` in `cli.c` is the pipeline owner: parse → place → analyse → output → clean up.
- `parse_ship_config` and `parse_cargo_list` in `parser.c` validate every numeric field through `safe_atof` and abort on the first bad value, nulling freed pointers to prevent use-after-free.
- `place_cargo_3d` in `placement_3d.c` sorts cargo largest-first, then for each item calls `find_best_fit_3d` (tightest-fit across all bins × spaces × orientations) and `split_space_3d` (guillotine split).
- `perform_analysis` in `analysis.c` runs a single-pass accumulation then five sequential physics calculations: hydrostatics → KG → GM + FSC → trim + heel → GZ/IMO → longitudinal strength.
- `ship_cleanup` in `analysis.c` frees all heap memory in the reverse order it was allocated, using NULL-after-free to make double-calls harmless.

*Next: [Frontier: classed stability software](47-frontier-classed-software.md).*
