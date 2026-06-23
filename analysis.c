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

void print_loading_plan(const Ship *ship) {
    AnalysisResult analysis = perform_analysis(ship);

    printf("\n--- CargoForge Stability Analysis ---\n\n");
    printf("Ship Specs: %.2f m × %.2f m | Max Weight: %.2f t\n", ship->length, ship->width, ship->max_weight / 1000.0f);
    printf("----------------------------------------------------------------------\n");

    if (isnan(analysis.gm)) {
        printf("  - 🔴 PLAN REJECTED: Total weight exceeds ship's maximum capacity.\n");
        return;
    }

    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x >= 0.0f) {
            printf("  - %-15s | Pos (X,Y,Z): (%7.2f, %7.2f, %7.2f) | %.2f t\n", c->id, c->pos_x, c->pos_y, c->pos_z, c->weight / 1000.0f);
        }
    }

    // Use a slightly wider range for "Good" balance
    const char *cg_stability_str = (analysis.cg.perc_x >= 45 && analysis.cg.perc_x <= 55 && analysis.cg.perc_y >= 45 && analysis.cg.perc_y <= 55) ? "Good" : "Warning";
    const char *gm_stability_str = (analysis.gm > 0.15f) ? "Stable" : "UNSTABLE";

    printf("\nLoad Summary\n");
    printf("  - Placed / Total items : %d / %d\n", analysis.placed_item_count, ship->cargo_count);
    printf("  - Total loaded weight  : %.2f t (%.1f %% of max)\n", analysis.total_cargo_weight_kg / 1000.0f, (ship->lightship_weight + analysis.total_cargo_weight_kg) / ship->max_weight * 100.0f);
    printf("  - Displacement / DWT   : %.2f t / %.2f t\n", analysis.displacement_t, analysis.deadweight_t);
    printf("  - CG (Lon / Trans)     : %.1f %% / %.1f %% | Balance: %s\n", analysis.cg.perc_x, analysis.cg.perc_y, cg_stability_str);

    printf("\nHydrostatics & Stability\n");
    printf("  - Draft (mean)         : %.2f m  (fore %.2f / aft %.2f)\n", analysis.draft_mean, analysis.draft_fore, analysis.draft_aft);
    printf("  - Displaced volume     : %.1f m³\n", analysis.volume_m3);
    printf("  - KG / KB / BM         : %.2f / %.2f / %.2f m\n", analysis.kg, analysis.kb, analysis.bm);
    printf("  - Trim / Heel          : %.2f m / %.1f°\n", analysis.trim, analysis.heel_deg);
    if (!isnan(analysis.freeboard))
        printf("  - Freeboard            : %.2f m\n", analysis.freeboard);
    printf("  - GM (transverse)      : %.2f m | Stability: %s\n", analysis.gm, gm_stability_str);
    printf("  - GML (longitudinal)   : %.2f m\n", analysis.gml);
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
