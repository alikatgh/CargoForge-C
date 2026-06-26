# The CLI and output formats

Every calculation this course has covered — displacement, GM, bin-packing, IMDG checks — reaches the user through a single entry point: the command-line interface in [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c). This lesson explains how the CLI is structured, how `getopt_long` turns raw `argv` into structured decisions, and why CargoForge-C offers five distinct output formats for the same underlying data.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **Subcommand dispatch** = "one binary that behaves differently depending on the first word you type after its name" — CargoForge-C reads `argv[1]` before anything else and routes it to one of six `cmd_*` functions (`cmd_optimize`, `cmd_validate`, `cmd_info`, etc.), exactly the way `git commit` and `git push` are the same binary doing different jobs.
- **`getopt_long`** = "the standard C machinery that turns `--format=json` into a variable you can use in your code" — CargoForge-C sets `optind = 2` before the parse loop so the library skips past the subcommand token and only sees the flags that follow it.
- **Configuration cascade** = "settings applied in layers, where later layers win" — CargoForge-C first reads `~/.cargoforgerc` (your global preferences), then `.cargoforgerc` in the current directory (project overrides), then honours whatever you typed on the command line; the CLI flag always beats the file.
- **`FORMAT_JSON` with `null` sentinels** = "a deliberate signal that says 'the ship was overweight so no stability numbers exist'" — when `perform_analysis` sets `result->gm` to `NAN`, `fprint_json_output` emits `"overweight": true` and replaces every hydrostatic field with `null`, so a downstream program never has to guess whether a zero means "zero" or "not computed".
- **stdout vs. stderr separation** = "data goes one place, status messages go another" — CargoForge-C writes all `[OK]` / `[WARNING]` / `[ERROR]` chatter to stderr and all formatted results to stdout, so you can pipe `cargoforge optimize ... --format json | jq` without the diagnostic lines polluting the JSON stream.
- **`--only-placed` / `--only-failed` filters** = "a way to narrow the table view to just the cargo rows you care about" — these flags are checked inside `output_table` before each `Cargo` item is printed, and they only apply to the `table` format because JSON and CSV are meant to be filtered programmatically by the consumer.

**Why it matters:** if you mix up which format to use — piping `human` output to `jq`, or checking the terminal when JSON has already nulled out all stability fields — you get garbled data or invisible failures; and if diagnostics leak into stdout, every downstream script breaks silently.

---

## The mental model 🧠

You'll forget the flag names — hold THIS picture instead:

> A ship's bridge has one wheel and one intercom. The wheel is `getopt_long` — it steers every flag into the right hand. The intercom is the subcommand dispatcher — one button for "optimize", one for "validate", one for "info". The five radio channels below deck (human, json, csv, table, markdown) are the output formats: same cargo data, different audience.

When you type `cargoforge optimize ship.cfg cargo.txt --format json`, the process flows in three clean stages. First, `argv[1]` ("optimize") hits the dispatch table and routes you to `cmd_optimize`. Second, `getopt_long` (with `optind = 2` to skip past the subcommand) turns `--format json` into `ctx->format = FORMAT_JSON`. Third, after `parse_cargo_list` and `perform_analysis` finish, `output_results` switches on that format enum and hands the same `Ship *` and `AnalysisResult *` to whichever formatter fits — `fprint_json_output` here, which will emit `null` sentinels if `result->gm` is `NAN`. One pipeline, one control structure.

---

<svg viewBox="0 0 620 280" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;
  font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>CargoForge-C CLI pipeline diagram</title>
  <desc>A box-and-arrow diagram showing argv flowing into the subcommand dispatcher, then through getopt_long option parsing, then through perform_analysis, and finally into one of five output format branches: human, json, csv, table, markdown.</desc>

  <!-- argv input box -->
  <rect x="10" y="110" width="100" height="44" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.55"/>
  <text x="60" y="128" text-anchor="middle" font-size="11" fill="currentColor" opacity="0.8">argv</text>
  <text x="60" y="143" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.6">argc, argv[]</text>

  <!-- arrow: argv → dispatcher -->
  <line x1="110" y1="132" x2="148" y2="132" stroke="currentColor" stroke-width="1.5" opacity="0.55"/>
  <polygon points="148,128 158,132 148,136" fill="currentColor" opacity="0.55"/>

  <!-- subcommand dispatcher box -->
  <rect x="158" y="98" width="118" height="68" rx="6"
        fill="none" stroke="#12A594" stroke-width="2"/>
  <text x="217" y="119" text-anchor="middle" font-size="11" font-weight="bold" fill="#12A594">dispatch_</text>
  <text x="217" y="133" text-anchor="middle" font-size="11" font-weight="bold" fill="#12A594">subcommand()</text>
  <text x="217" y="152" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">reads argv[1]</text>

  <!-- arrow: dispatcher → getopt -->
  <line x1="276" y1="132" x2="314" y2="132" stroke="currentColor" stroke-width="1.5" opacity="0.55"/>
  <polygon points="314,128 324,132 314,136" fill="currentColor" opacity="0.55"/>

  <!-- getopt_long box -->
  <rect x="324" y="98" width="118" height="68" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="383" y="119" text-anchor="middle" font-size="11" fill="currentColor">getopt_long</text>
  <text x="383" y="133" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">optind = 2</text>
  <text x="383" y="148" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">--format, --output…</text>
  <text x="383" y="161" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.6">→ CLIContext</text>

  <!-- arrow: getopt → output_results -->
  <line x1="442" y1="132" x2="480" y2="132" stroke="currentColor" stroke-width="1.5" opacity="0.55"/>
  <polygon points="480,128 490,132 480,136" fill="currentColor" opacity="0.55"/>

  <!-- output_results box -->
  <rect x="490" y="98" width="118" height="68" rx="6"
        fill="none" stroke="currentColor" stroke-width="1.5" opacity="0.7"/>
  <text x="549" y="119" text-anchor="middle" font-size="11" fill="currentColor">output_results()</text>
  <text x="549" y="133" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">switch(ctx-&gt;format)</text>
  <text x="549" y="148" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.6">Ship * + AnalysisResult *</text>

  <!-- Five format output branches (fan-out downward from output_results) -->
  <!-- vertical stem from output_results bottom -->
  <line x1="549" y1="166" x1="549" y1="190" x2="549" y2="200"
        stroke="currentColor" stroke-width="1.2" opacity="0.45"/>

  <!-- horizontal bar connecting all five branches -->
  <line x1="84" y1="200" x2="596" y2="200"
        stroke="currentColor" stroke-width="1.2" opacity="0.35"/>

  <!-- stems down to each format label -->
  <!-- human: x=84 -->
  <line x1="84" y1="200" x2="84" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <rect x="30" y="220" width="108" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <text x="84" y="238" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">human (default)</text>

  <!-- json: x=212 -->
  <line x1="212" y1="200" x2="212" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <rect x="162" y="220" width="100" height="28" rx="4"
        fill="none" stroke="#12A594" stroke-width="1.5"/>
  <text x="212" y="238" text-anchor="middle" font-size="10" fill="#12A594">json (+ null sentinel)</text>

  <!-- csv: x=340 -->
  <line x1="340" y1="200" x2="340" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <rect x="294" y="220" width="92" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <text x="340" y="238" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">csv</text>

  <!-- table: x=460 -->
  <line x1="460" y1="200" x2="460" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <rect x="410" y="220" width="100" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <text x="460" y="238" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">table (+ filters)</text>

  <!-- markdown: x=566 -->
  <line x1="566" y1="200" x2="566" y2="220" stroke="currentColor" stroke-width="1.2" opacity="0.35"/>
  <rect x="516" y="220" width="100" height="28" rx="4"
        fill="none" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <text x="566" y="238" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.8">markdown</text>

  <!-- stderr label at bottom -->
  <text x="84" y="268" text-anchor="middle" font-size="9" fill="#D05663" opacity="0.85">stdout</text>
  <text x="310" y="275" text-anchor="middle" font-size="9" fill="#D05663" opacity="0.7">diagnostics → stderr (always)</text>

  <!-- top label -->
  <text x="310" y="22" text-anchor="middle" font-size="12" font-weight="bold" fill="currentColor" opacity="0.75">CargoForge-C CLI pipeline</text>
  <text x="310" y="40" text-anchor="middle" font-size="10" fill="currentColor" opacity="0.5">cargoforge optimize ship.cfg cargo.txt --format json</text>
</svg>

## Subcommands: one binary, many jobs

CargoForge-C uses a **subcommand dispatch** pattern, the same model as `git` or `cargo`. A single binary handles conceptually different tasks by reading `argv[1]` before any option parsing begins.

From [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c), the dispatch table is a simple chain of `strcmp` calls:

```c
// src/cli.c  — dispatch_subcommand()
if (strcmp(ctx->subcommand, "optimize") == 0) return cmd_optimize(ctx);
if (strcmp(ctx->subcommand, "validate") == 0) return cmd_validate(ctx);
if (strcmp(ctx->subcommand, "info") == 0)     return cmd_info(ctx);
if (strcmp(ctx->subcommand, "serve") == 0)    return cmd_serve(ctx);
if (strcmp(ctx->subcommand, "version") == 0)  return cmd_version(ctx);
if (strcmp(ctx->subcommand, "help") == 0)     return cmd_help(ctx);
```

Each branch calls a dedicated function that does one coherent job. The return value of every `cmd_*` function is an exit code — `EXIT_SUCCESS` (0), `EXIT_PARSE_ERROR`, `EXIT_INVALID_ARGS`, or `EXIT_VALIDATION_ERROR` — so the shell can check `$?` and use CargoForge-C in pipelines and CI scripts.

### What each subcommand does

| Subcommand | Inputs | What it runs |
|---|---|---|
| `optimize` | ship config + cargo manifest | parse → place (`place_cargo_3d`) → analyse (`perform_analysis`) → output |
| `validate` | ship config + cargo manifest | parse only; reports errors, never optimises |
| `info` | ship config + optional manifest | prints dimensions, weights, cargo summary without placement |
| `serve` | none (optional `--port=N`) | starts the JSON-RPC 2.0 HTTP server |
| `version` | none | prints version string and build date |
| `help` | optional subcommand name | prints general help or per-subcommand usage |

`info` is unique in that the cargo manifest is **optional** — it happily reports ship dimensions alone. This is deliberate: you might want to inspect a ship config before you have a manifest ready.

```c
// src/cli.c  — cmd_info()
if (ctx->cargo_file) {
    if (parse_cargo_list(ctx->cargo_file, &ship) != 0) { ... }
}
output_ship_info(&ship, ctx->format);
```

---

## Argument parsing with `getopt_long`

POSIX defines `getopt` for short options (`-v`, `-f json`). The GNU extension `getopt_long` adds long-form options (`--verbose`, `--format=json`). CargoForge-C uses the long form throughout for readability.

### The options table

`getopt_long` needs a `struct option` array that maps long names to their short equivalents and specifies whether they take an argument:

```c
// src/cli.c  — parse_cli_args()
static struct option long_options[] = {
    {"help",        no_argument,       0, 'h'},
    {"verbose",     no_argument,       0, 'v'},
    {"quiet",       no_argument,       0, 'q'},
    {"format",      required_argument, 0, 'f'},
    {"output",      required_argument, 0, 'o'},
    {"no-viz",      no_argument,       0, 'n'},
    {"no-color",    no_argument,       0, 'c'},
    {"only-placed", no_argument,       0, 'p'},
    {"only-failed", no_argument,       0, 'F'},
    {"type",        required_argument, 0, 't'},
    {"json",        no_argument,       0, 'j'},
    {0, 0, 0, 0}   /* sentinel — marks the end of the array */
};
```

Three columns matter here:

- **`no_argument`** — the flag is a boolean switch; no value follows.
- **`required_argument`** — the next token (or the part after `=`) is consumed as `optarg`.
- The final `0` field is set when `getopt_long` should store the result in an `int *` variable instead of returning it — CargoForge-C doesn't use that mode, so it's always `0`.

The `getopt_long` call in the parse loop:

```c
optind = 2;   /* skip argv[0] (program name) and argv[1] (subcommand) */
int opt;
while ((opt = getopt_long(argc, argv, "hvqf:o:t:", long_options, NULL)) != -1) {
    switch (opt) {
        case 'f':
            if (strcmp(optarg, "json") == 0) ctx->format = FORMAT_JSON;
            /* ... */
            break;
        case 'o': ctx->output_file = optarg; break;
        case 'n': ctx->show_viz = false; break;
        /* ... */
    }
}
```

Setting `optind = 2` before the loop is the subcommand trick: it tells `getopt_long` to treat the subcommand token as already consumed and start scanning from `argv[2]` onward. After the loop, any remaining non-option tokens (the positional file arguments) are consumed manually:

```c
if (optind < argc) ctx->ship_file  = argv[optind++];
if (optind < argc) ctx->cargo_file = argv[optind++];
```

!!! note
    `optarg` is a global pointer into the original `argv` array. It is valid as long as you do not modify `argv`. CargoForge-C stores it directly in `CLIContext` fields (e.g. `ctx->output_file = optarg`) rather than copying, which is safe because `argv` lives for the entire process lifetime.

---

## Configuration cascade: `.cargoforgerc`

Before parsing any flags, `init_cli_context` reads two optional config files:

```c
// src/cli.c  — init_cli_context()
char *home = getenv("HOME");
if (home) {
    snprintf(global_config, sizeof(global_config), "%s/.cargoforgerc", home);
    load_config_file(ctx, global_config);
}
load_config_file(ctx, ".cargoforgerc");
```

The pattern is **global defaults → project-local override → CLI flags**. A project-level `.cargoforgerc` in the current directory overrides the user-global one; explicit `--format=json` on the command line wins over both. This is the standard Unix convention for tool configuration.

`load_config_file` is a simple key=value parser that recognises `format`, `color`, `verbose`, `quiet`, and `show_viz`. If the file does not exist, `fopen` returns NULL and the function returns immediately — no error.

---

## The five output formats

All five are dispatched from `output_results` in [`src/cli.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/cli.c). The same `Ship *` and `AnalysisResult *` objects are passed to every formatter; what changes is only the representation:

```c
// src/cli.c  — output_results()
switch (format) {
    case FORMAT_JSON:     print_json_output(ship, result);          break;
    case FORMAT_CSV:      output_csv(ship, result, fp);             break;
    case FORMAT_TABLE:    output_table(ship, result, fp);           break;
    case FORMAT_MARKDOWN: output_markdown(ship, result, fp);        break;
    case FORMAT_HUMAN:
    default:
        print_loading_plan(ship);
        if (g_ctx && g_ctx->show_viz) {
            print_cargo_layout_ascii(ship);
            print_cargo_summary(ship);
        }
        break;
}
```

### FORMAT_HUMAN (default)

The human format calls `print_loading_plan` (defined in [`src/analysis.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/analysis.c)), which walks every stability number in plain prose. If `show_viz` is set (default: true) it appends an ASCII plan-view of the cargo layout. This is the format you use at the terminal when inspecting a loading plan interactively.

### FORMAT_JSON

Delegated to `fprint_json_output` in [`src/json_output.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/json_output.c). The output is a single JSON object with three top-level keys: `"ship"`, `"cargo"` (an array), and `"analysis"`.

The `"analysis"` block contains a critical branch: if `result->gm` is `NAN` (set when the ship is overweight and analysis was rejected), all hydrostatic and stability fields are emitted as `null`:

```c
// src/json_output.c  — fprint_json_output()
if (!isnan(result->gm)) {
    fprintf(fp, "    \"hydrostatics\": {\n");
    fprintf(fp, "      \"draft\": %.3f,\n", result->draft);
    /* ... all stability fields ... */
    fprintf(fp, "    \"overweight\": false\n");
} else {
    fprintf(fp, "    \"hydrostatics\": null,\n");
    fprintf(fp, "    \"trim\": null,\n");
    fprintf(fp, "    \"imo_stability\": null,\n");
    fprintf(fp, "    \"overweight\": true\n");
}
```

This means a JSON consumer does not need to distinguish between "field not computed" and "field zero" — it simply checks `overweight` first and skips the null subtree if true.

The `escape_json_string` helper in [`src/json_output.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/json_output.c) escapes `"` and `\` characters inside string fields so that cargo IDs containing those characters do not produce malformed JSON.

JSON is the format of choice when piping results to another program, calling the CLI from a web backend, or archiving results for later analysis.

### FORMAT_CSV

`output_csv` writes one header row then one data row per cargo item:

```c
// src/cli.c  — output_csv()
fprintf(fp, "ID,Weight_kg,Length_m,Width_m,Height_m,Type,Placed,Pos_X,Pos_Y,Pos_Z\n");
for (int i = 0; i < ship->cargo_count; i++) {
    Cargo *c = &ship->cargo[i];
    fprintf(fp, "%s,%.2f,%.2f,%.2f,%.2f,%s,%s,%.2f,%.2f,%.2f\n",
            c->id, c->weight,
            c->dimensions[0], c->dimensions[1], c->dimensions[2],
            c->type, (c->pos_x >= 0) ? "yes" : "no",
            c->pos_x, c->pos_y, c->pos_z);
}
```

Notice what CSV omits: there is no stability summary row. CSV is cargo-item-centric — one row per item, suitable for importing into a spreadsheet or database. The `Placed` column (`yes`/`no`) lets you filter placed vs. unplaced items with a simple spreadsheet filter. Unplaced items still appear with their `pos_x = -1.0` sentinel, so nothing is silently discarded.

### FORMAT_TABLE

`output_table` is the terminal-friendly columnar view. It applies the `--only-placed`, `--only-failed`, and `--type` filters, which are stored in `g_ctx`:

```c
// src/cli.c  — output_table()
if (g_ctx && g_ctx->only_placed && c->pos_x < 0) continue;
if (g_ctx && g_ctx->only_failed && c->pos_x >= 0) continue;
if (g_ctx && g_ctx->cargo_type_filter &&
    strcmp(c->type, g_ctx->cargo_type_filter) != 0) continue;
```

These filters are only active in table mode because CSV and JSON are designed for downstream processing where you filter programmatically. After the cargo rows, a summary line prints placed count, total weight in tonnes, and raw GM.

### FORMAT_MARKDOWN

`output_markdown` generates a GitHub-renderable report. It produces three sections: Ship Specifications (a two-column table), Cargo Placement (a full item table), and Stability Analysis (a bullet list). If a free-surface correction is present, it reports both raw and corrected GM:

```c
// src/cli.c  — output_markdown()
if (result->free_surface_correction > 0.001f) {
    fprintf(fp, "- **Free Surface Correction:** -%.3f m\n",
            result->free_surface_correction);
    fprintf(fp, "- **GM (corrected):** %.2f m\n", result->gm_corrected);
}
```

Markdown is designed for reports — generated in CI, committed to a repository, or attached to a voyage plan. You would not parse it programmatically; use JSON for that.

### Choosing a format

| Use case | Format |
|---|---|
| Interactive inspection at the terminal | `human` (default) |
| Piping into another program or web API | `json` |
| Importing into a spreadsheet | `csv` |
| Quick scan of many items, with filtering | `table` |
| CI artifact, GitHub PR comment, voyage report | `markdown` |

### Writing to a file

The `--output=FILE` option redirects the chosen format to a file:

```c
// src/cli.c  — output_results()
if (output_file) {
    fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
        fp = stdout;  /* fall back gracefully */
    }
}
```

Diagnostic messages (`[OK]`, `[WARNING]`, `[ERROR]`) are always written to **stderr**, not stdout. This separation means you can redirect stdout to a file or pipe it to `jq` without mixing status chatter into the data stream.

---

## Colored output

Color is enabled automatically when stderr is a terminal:

```c
// src/cli.c  — init_cli_context()
ctx->color = isatty(STDERR_FILENO);
```

`isatty` returns 1 when the file descriptor is connected to a terminal (a TTY), and 0 when it is a pipe or file. This is the standard way to auto-disable ANSI escape codes in pipelines. The user can override with `--no-color` or by setting `color = false` in `.cargoforgerc`.

---

## Recap

- CargoForge-C uses a **subcommand dispatch** pattern: `argv[1]` selects one of six commands; each has its own `cmd_*` function and exit code.
- `getopt_long` with `optind = 2` handles flags after the subcommand; remaining positional args are the input file paths.
- Configuration cascades from `~/.cargoforgerc` → `.cargoforgerc` → CLI flags, following Unix convention.
- Five output formats serve different consumers: `human` for terminals, `json` for programs, `csv` for spreadsheets, `table` for filtered scanning, `markdown` for reports.
- In JSON output, an overweight rejection sets `"overweight": true` and all stability fields to `null`, giving consumers a clean sentinel rather than a NAN.
- Diagnostics go to stderr; data goes to stdout — keeping the streams separable for pipelines.

*Next: [The library and public API](43-the-library-and-public-api.md).*
