/*
 * analysis.c - Post-optimization analysis and reporting.
 */
#include <math.h>
#include <stdbool.h>
#include "cargoforge.h"

#define SEAWATER_DENSITY 1.025f
#define WATERPLANE_COEFFICIENT 0.8f
#define RAD_TO_DEG 57.29577951f

AnalysisResult perform_analysis(const Ship *ship) {
    AnalysisResult result = {0};
    result.freeboard = NAN; // remains NAN unless a hull depth is configured

    // Calculate moments for all placed cargo
    float cargo_moment_x = 0.0f, cargo_moment_y = 0.0f;
    float vertical_moment = ship->lightship_weight * ship->lightship_kg;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue; // Skip unplaced cargo

        result.placed_item_count++;
        result.total_cargo_weight_kg += c->weight;

        // THE FIX: Use the final placed dimensions for an accurate moment calculation
        cargo_moment_x += c->weight * (c->pos_x + c->placed_w / 2.0f);
        cargo_moment_y += c->weight * (c->pos_y + c->placed_h / 2.0f);
        vertical_moment += c->weight * (c->pos_z + c->dimensions[2] / 2.0f);
    }

    float total_ship_weight_kg = ship->lightship_weight + result.total_cargo_weight_kg;

    // Weights are meaningful even for an overweight plan (that's why it's rejected).
    result.displacement_t = total_ship_weight_kg / 1000.0f;
    result.deadweight_t = result.total_cargo_weight_kg / 1000.0f;

    if (total_ship_weight_kg > ship->max_weight) {
        // Overweight: the hydrostatics below are not meaningful, mark them invalid.
        result.cg.perc_x = result.cg.perc_y = NAN;
        result.gm = result.gml = NAN;
        result.kg = result.kb = result.bm = NAN;
        result.draft_mean = result.draft_fore = result.draft_aft = NAN;
        result.trim = result.heel_deg = result.volume_m3 = NAN;
        return result;
    }

    // Combine ship's own moment with cargo moment for the final CG.
    float total_moment_x = (ship->lightship_weight * ship->length / 2.0f) + cargo_moment_x;
    float total_moment_y = (ship->lightship_weight * ship->width / 2.0f) + cargo_moment_y;
    float lcg = ship->length / 2.0f, tcg = ship->width / 2.0f;
    if (total_ship_weight_kg > 0.01f) {
        lcg = total_moment_x / total_ship_weight_kg;
        tcg = total_moment_y / total_ship_weight_kg;
        result.cg.perc_x = lcg / ship->length * 100.0f;
        result.cg.perc_y = tcg / ship->width * 100.0f;
    }

    // --- Hydrostatics: draft, KB, BM, transverse GM ---
    float final_kg = vertical_moment / total_ship_weight_kg;
    float displaced_volume = (total_ship_weight_kg / 1000.0f) / SEAWATER_DENSITY;
    float draft = displaced_volume / (ship->length * ship->width * WATERPLANE_COEFFICIENT);
    float kb = draft / 2.0f;
    float inertia_t = (ship->length * pow(ship->width, 3)) / 12.0f * WATERPLANE_COEFFICIENT;
    float bm = inertia_t / displaced_volume;

    result.kg = final_kg;
    result.kb = kb;
    result.bm = bm;
    result.gm = kb + bm - final_kg;
    result.draft_mean = draft;
    result.volume_m3 = displaced_volume;

    // --- Longitudinal metacentric height (uses the longitudinal waterplane inertia) ---
    float inertia_l = (ship->width * pow(ship->length, 3)) / 12.0f * WATERPLANE_COEFFICIENT;
    float bml = inertia_l / displaced_volume;
    result.gml = kb + bml - final_kg;

    // --- Trim (simplified): driven by the LCG offset from amidships (LCB ~ midships) ---
    float trim = 0.0f;
    if (result.gml > 0.01f) {
        trim = (lcg - ship->length / 2.0f) * ship->length / result.gml;
    }
    result.trim = trim;
    result.draft_fore = draft - trim / 2.0f;
    result.draft_aft = draft + trim / 2.0f;

    // --- Static list from the transverse CG offset: tan(heel) = offset / GM ---
    if (result.gm > 0.01f) {
        float tcg_offset = tcg - ship->width / 2.0f;
        result.heel_deg = atanf(tcg_offset / result.gm) * RAD_TO_DEG;
    } else {
        result.heel_deg = NAN; // unstable: no static equilibrium heel angle
    }

    // --- Freeboard (only if a hull depth was configured) ---
    if (ship->depth > 0.0f) {
        result.freeboard = ship->depth - draft;
    }

    return result;
}

/* ANSI color codes — only emitted when OutputOptions.color is set. */
#define C_RESET  "\033[0m"
#define C_BOLD   "\033[1m"
#define C_RED    "\033[1;31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_DIM    "\033[2m"

// Returns `code` when color is enabled, otherwise the empty string.
static const char *col(const OutputOptions *o, const char *code) {
    return o->color ? code : "";
}

// Renders one top-down layer (holds when deck=false, the deck when deck=true) as
// an ASCII grid, scaling the ship's length to ~58 columns and preserving aspect.
static void render_layer(const Ship *ship, bool deck) {
    const int cols = 58;
    int rows = (int)(cols * ship->width / ship->length + 0.5f);
    if (rows < 4) rows = 4;
    if (rows > 16) rows = 16;

    char grid[16][59]; // cols (58) + NUL; fixed size avoids a VLA
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) grid[r][c] = ' ';
        grid[r][cols] = '\0';
    }

    float sx = cols / ship->length, sy = rows / ship->width;

    // Hold dividers (only on the below-deck layer).
    if (!deck) {
        int holds = ship->hold_count > 0 ? ship->hold_count : DEFAULT_HOLDS;
        for (int h = 1; h < holds; h++) {
            int x = (int)(h * (ship->length / holds) * sx);
            if (x >= 0 && x < cols)
                for (int r = 0; r < rows; r++) grid[r][x] = ':';
        }
    }

    // Cargo footprints, labelled by the item's first character.
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0.0f || (c->pos_z >= 0.0f) != deck) continue;
        int x0 = (int)(c->pos_x * sx), x1 = (int)((c->pos_x + c->placed_w) * sx);
        int y0 = (int)(c->pos_y * sy), y1 = (int)((c->pos_y + c->placed_h) * sy);
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        char ch = c->id[0];
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        for (int y = y0; y < y1 && y < rows; y++)
            for (int x = x0; x < x1 && x < cols; x++) grid[y][x] = ch;
    }

    printf("   +"); for (int c = 0; c < cols; c++) putchar('-'); printf("+\n");
    for (int r = 0; r < rows; r++) printf("   |%s|\n", grid[r]);
    printf("   +"); for (int c = 0; c < cols; c++) putchar('-'); printf("+\n");
}

// Prints a `[####    ] NN%` bar, optionally colored by fill level.
static void print_bar(const OutputOptions *opt, float pct) {
    const int width = 20;
    int filled = (int)(pct / 100.0f * width + 0.5f);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    const char *c = pct > 90.0f ? C_RED : (pct > 60.0f ? C_YELLOW : C_GREEN);
    printf("[%s", col(opt, c));
    for (int i = 0; i < width; i++) putchar(i < filled ? '#' : ' ');
    printf("%s] %3.0f%%\n", col(opt, C_RESET), pct);
}

// Top-down stowage plan + per-hold utilization bars + a GM stability gauge.
static void print_ship_diagram(const Ship *ship, const OutputOptions *opt, const AnalysisResult *a) {
    int holds = ship->hold_count > 0 ? ship->hold_count : DEFAULT_HOLDS;

    printf("\nStowage Plan (top-down, bow → stern left→right)\n");
    printf(" Holds (below deck):\n");
    render_layer(ship, false);

    // Only draw the deck panel if anything is stowed on deck.
    int deck_items = 0;
    for (int i = 0; i < ship->cargo_count; i++)
        if (ship->cargo[i].pos_x >= 0.0f && ship->cargo[i].pos_z >= 0.0f) deck_items++;
    if (deck_items > 0) {
        printf(" Deck:\n");
        render_layer(ship, true);
    }

    // Per-hold (and deck) area utilization.
    printf("\nUtilization\n");
    float hold_len = ship->length / holds;
    float hold_area = hold_len * ship->width;
    for (int h = 0; h < holds; h++) {
        float used = 0.0f;
        for (int i = 0; i < ship->cargo_count; i++) {
            const Cargo *c = &ship->cargo[i];
            if (c->pos_x < 0.0f || c->pos_z >= 0.0f) continue;
            float cx = c->pos_x + c->placed_w / 2.0f;
            if (cx >= h * hold_len && cx < (h + 1) * hold_len) used += c->placed_w * c->placed_h;
        }
        printf("  Hold %-2d ", h + 1);
        print_bar(opt, hold_area > 0.0f ? used / hold_area * 100.0f : 0.0f);
    }
    if (deck_items > 0) {
        float used = 0.0f, deck_area = ship->length * ship->width;
        for (int i = 0; i < ship->cargo_count; i++) {
            const Cargo *c = &ship->cargo[i];
            if (c->pos_x >= 0.0f && c->pos_z >= 0.0f) used += c->placed_w * c->placed_h;
        }
        printf("  Deck    ");
        print_bar(opt, deck_area > 0.0f ? used / deck_area * 100.0f : 0.0f);
    }

    // GM stability gauge: fill toward a comfortable 5 m, marker '>' clamps at top.
    if (!isnan(a->gm)) {
        const int width = 20;
        float frac = a->gm / 5.0f;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        int filled = (int)(frac * width + 0.5f);
        bool stable = a->gm > 0.15f;
        printf("  GM      [%s", col(opt, stable ? C_GREEN : C_RED));
        for (int i = 0; i < width; i++) putchar(i < filled ? '=' : ' ');
        printf("%s] %.2f m %s\n", col(opt, C_RESET), a->gm, stable ? "Stable" : "UNSTABLE");
    }
}

void print_loading_plan(const Ship *ship, const OutputOptions *opt) {
    OutputOptions defaults = {false, 0, false};
    if (!opt) opt = &defaults;
    AnalysisResult analysis = perform_analysis(ship);

    printf("\n%s--- CargoForge Stability Analysis ---%s\n\n", col(opt, C_BOLD), col(opt, C_RESET));
    printf("Ship Specs: %.2f m × %.2f m | Max Weight: %.2f t\n", ship->length, ship->width, ship->max_weight / 1000.0f);
    printf("----------------------------------------------------------------------\n");

    if (isnan(analysis.gm)) {
        printf("  - %s🔴 PLAN REJECTED: Total weight exceeds ship's maximum capacity.%s\n",
               col(opt, C_RED), col(opt, C_RESET));
        return;
    }

    // Per-item placement list (suppressed in quiet mode).
    if (opt->verbosity >= 0) {
        for (int i = 0; i < ship->cargo_count; ++i) {
            const Cargo *c = &ship->cargo[i];
            if (c->pos_x < 0.0f) continue;
            printf("  - %-15s | Pos (X,Y,Z): (%7.2f, %7.2f, %7.2f) | %.2f t",
                   c->id, c->pos_x, c->pos_y, c->pos_z, c->weight / 1000.0f);
            if (opt->verbosity >= 1 && analysis.total_cargo_weight_kg > 0.0f) {
                float share = c->weight / analysis.total_cargo_weight_kg * 100.0f;
                float lon = (c->pos_x + c->placed_w / 2.0f) / ship->length * 100.0f;
                printf("  %s[%.1f%% of cargo, Lon %.0f%%]%s", col(opt, C_DIM), share, lon, col(opt, C_RESET));
            }
            printf("\n");
        }
    }

    // Use a slightly wider range for "Good" balance.
    bool balanced = (analysis.cg.perc_x >= 45 && analysis.cg.perc_x <= 55 &&
                     analysis.cg.perc_y >= 45 && analysis.cg.perc_y <= 55);
    bool stable = (analysis.gm > 0.15f);
    const char *bal_col = balanced ? C_GREEN : C_YELLOW;
    const char *stab_col = stable ? C_GREEN : C_RED;

    printf("\nLoad Summary\n");
    printf("  - Placed / Total items : %d / %d\n", analysis.placed_item_count, ship->cargo_count);
    printf("  - Total loaded weight  : %.2f t (%.1f %% of max)\n", analysis.total_cargo_weight_kg / 1000.0f, (ship->lightship_weight + analysis.total_cargo_weight_kg) / ship->max_weight * 100.0f);
    printf("  - Displacement / DWT   : %.2f t / %.2f t\n", analysis.displacement_t, analysis.deadweight_t);
    printf("  - CG (Lon / Trans)     : %.1f %% / %.1f %% | Balance: %s%s%s\n",
           analysis.cg.perc_x, analysis.cg.perc_y, col(opt, bal_col), balanced ? "Good" : "Warning", col(opt, C_RESET));

    // Full hydrostatics block (suppressed in quiet mode).
    if (opt->verbosity >= 0) {
        printf("\nHydrostatics & Stability\n");
        printf("  - Draft (mean)         : %.2f m  (fore %.2f / aft %.2f)\n", analysis.draft_mean, analysis.draft_fore, analysis.draft_aft);
        printf("  - Displaced volume     : %.1f m³\n", analysis.volume_m3);
        printf("  - KG / KB / BM         : %.2f / %.2f / %.2f m\n", analysis.kg, analysis.kb, analysis.bm);
        printf("  - Trim / Heel          : %.2f m / %.1f°\n", analysis.trim, analysis.heel_deg);
        if (!isnan(analysis.freeboard))
            printf("  - Freeboard            : %.2f m\n", analysis.freeboard);
    }
    printf("  - GM (transverse)      : %.2f m | Stability: %s%s%s\n",
           analysis.gm, col(opt, stab_col), stable ? "Stable" : "UNSTABLE", col(opt, C_RESET));
    if (opt->verbosity >= 0)
        printf("  - GML (longitudinal)   : %.2f m\n", analysis.gml);

    if (opt->diagram && opt->verbosity >= 0)
        print_ship_diagram(ship, opt, &analysis);
}

// Prints `    "key": <value>,\n`, emitting JSON null when the value is NaN.
static void json_num(const char *key, float v, bool last) {
    if (isnan(v)) printf("    \"%s\": null%s\n", key, last ? "" : ",");
    else printf("    \"%s\": %.2f%s\n", key, v, last ? "" : ",");
}

// Writes a minimally-escaped JSON string literal (handles " \ and control chars).
static void print_json_string(const char *s) {
    putchar('"');
    for (; *s; ++s) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { putchar('\\'); putchar(ch); }
        else if (ch < 0x20) printf("\\u%04x", ch);
        else putchar(ch);
    }
    putchar('"');
}

// Emits the loading plan and stability summary as a single JSON object on stdout,
// so the tool can be composed into scripts and pipelines.
void print_loading_plan_json(const Ship *ship) {
    AnalysisResult a = perform_analysis(ship);
    bool rejected = isnan(a.gm);

    printf("{\n");
    printf("  \"ship\": {\"length_m\": %.2f, \"width_m\": %.2f, \"max_weight_t\": %.2f},\n",
           ship->length, ship->width, ship->max_weight / 1000.0f);

    printf("  \"placements\": [");
    int printed = 0;
    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0.0f) continue;
        printf("%s\n    {\"id\": ", printed ? "," : "");
        print_json_string(c->id);
        printf(", \"x\": %.2f, \"y\": %.2f, \"z\": %.2f, \"weight_t\": %.2f}",
               c->pos_x, c->pos_y, c->pos_z, c->weight / 1000.0f);
        ++printed;
    }
    printf("%s],\n", printed ? "\n  " : "");

    bool balanced = (a.cg.perc_x >= 45.0f && a.cg.perc_x <= 55.0f &&
                     a.cg.perc_y >= 45.0f && a.cg.perc_y <= 55.0f);
    bool stable = (a.gm > 0.15f);
    float pct_max = (ship->lightship_weight + a.total_cargo_weight_kg) / ship->max_weight * 100.0f;

    printf("  \"summary\": {\n");
    printf("    \"placed\": %d,\n", a.placed_item_count);
    printf("    \"total\": %d,\n", ship->cargo_count);
    printf("    \"rejected\": %s,\n", rejected ? "true" : "false");
    printf("    \"loaded_weight_t\": %.2f,\n", a.total_cargo_weight_kg / 1000.0f);
    printf("    \"percent_of_max\": %.1f,\n", pct_max);
    json_num("displacement_t", a.displacement_t, false);
    json_num("deadweight_t", a.deadweight_t, false);
    json_num("cg_longitudinal_pct", a.cg.perc_x, false);
    json_num("cg_transverse_pct", a.cg.perc_y, false);
    json_num("kg_m", a.kg, false);
    json_num("kb_m", a.kb, false);
    json_num("bm_m", a.bm, false);
    json_num("gm_m", a.gm, false);
    json_num("gml_m", a.gml, false);
    json_num("draft_mean_m", a.draft_mean, false);
    json_num("draft_fore_m", a.draft_fore, false);
    json_num("draft_aft_m", a.draft_aft, false);
    json_num("trim_m", a.trim, false);
    json_num("heel_deg", a.heel_deg, false);
    json_num("volume_m3", a.volume_m3, false);
    json_num("freeboard_m", a.freeboard, false);
    printf("    \"balanced\": %s,\n", (!rejected && balanced) ? "true" : "false");
    printf("    \"stable\": %s\n", stable ? "true" : "false");
    printf("  }\n");
    printf("}\n");
}

// Writes one CSV field, quoting per RFC 4180 if it contains a comma, quote, CR or LF.
static void print_csv_field(const char *s) {
    bool needs_quote = false;
    for (const char *p = s; *p; ++p)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') { needs_quote = true; break; }
    if (!needs_quote) { fputs(s, stdout); return; }
    putchar('"');
    for (const char *p = s; *p; ++p) { if (*p == '"') putchar('"'); putchar(*p); }
    putchar('"');
}

// Emits the placements as RFC-4180 CSV (header + one row per placed item).
void print_loading_plan_csv(const Ship *ship) {
    printf("id,x_m,y_m,z_m,weight_t,footprint_w_m,footprint_h_m,type\n");
    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0.0f) continue;
        print_csv_field(c->id);
        printf(",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,", c->pos_x, c->pos_y, c->pos_z,
               c->weight / 1000.0f, c->placed_w, c->placed_h);
        print_csv_field(c->type);
        putchar('\n');
    }
}

// Escapes a Markdown table cell (just the pipe, which would break the table).
static void print_md_cell(const char *s) {
    for (const char *p = s; *p; ++p) { if (*p == '|') putchar('\\'); putchar(*p); }
}

// Emits a full Markdown report: ship, placements table, and a stability summary.
void print_loading_plan_md(const Ship *ship) {
    AnalysisResult a = perform_analysis(ship);
    bool rejected = isnan(a.gm);

    printf("# CargoForge Stowage Plan\n\n");
    printf("**Ship:** %.2f m × %.2f m  ·  **Max weight:** %.2f t\n\n",
           ship->length, ship->width, ship->max_weight / 1000.0f);

    if (rejected) {
        printf("> ⚠️ **PLAN REJECTED** — total weight exceeds the ship's maximum.\n");
        return;
    }

    printf("## Placements\n\n");
    printf("| ID | X (m) | Y (m) | Z (m) | Weight (t) | Type |\n");
    printf("|----|------:|------:|------:|-----------:|------|\n");
    float heaviest = 0.0f, lightest = 0.0f;
    bool first = true;
    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0.0f) continue;
        float t = c->weight / 1000.0f;
        if (first || t > heaviest) heaviest = t;
        if (first || t < lightest) lightest = t;
        first = false;
        printf("| ");
        print_md_cell(c->id);
        printf(" | %.2f | %.2f | %.2f | %.2f | ", c->pos_x, c->pos_y, c->pos_z, t);
        print_md_cell(c->type);
        printf(" |\n");
    }

    float avg = a.placed_item_count > 0 ? (a.total_cargo_weight_kg / 1000.0f) / a.placed_item_count : 0.0f;
    printf("\n## Summary\n\n");
    printf("- **Placed / total:** %d / %d\n", a.placed_item_count, ship->cargo_count);
    printf("- **Loaded / displacement:** %.2f t / %.2f t\n", a.deadweight_t, a.displacement_t);
    printf("- **Cargo weight (avg / heaviest / lightest):** %.2f / %.2f / %.2f t\n", avg, heaviest, lightest);
    printf("- **CG (long. / trans.):** %.1f%% / %.1f%%\n", a.cg.perc_x, a.cg.perc_y);
    printf("- **Draft (mean):** %.2f m\n", a.draft_mean);
    printf("- **GM / GML:** %.2f m / %.2f m\n", a.gm, a.gml);
    printf("- **Stability:** %s\n", a.gm > 0.15f ? "Stable ✅" : "UNSTABLE ❌");
}
