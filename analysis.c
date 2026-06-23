/*
 * analysis.c - Post-optimization analysis and reporting.
 */
#include <math.h>
#include <stdbool.h>
#include "cargoforge.h"

#define SEAWATER_DENSITY 1.025f
#define WATERPLANE_COEFFICIENT 0.8f

AnalysisResult perform_analysis(const Ship *ship) {
    AnalysisResult result = {{0.0f, 0.0f}, 0.0f, 0.0f, 0};

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

    if (total_ship_weight_kg > ship->max_weight) {
        result.gm = NAN; // Mark as invalid if overweight
        return result;
    }
    
    // Combine ship's own moment with cargo moment for the final CG
    float total_moment_x = (ship->lightship_weight * ship->length / 2.0f) + cargo_moment_x;
    float total_moment_y = (ship->lightship_weight * ship->width / 2.0f) + cargo_moment_y;

    if (total_ship_weight_kg > 0.01f) {
        result.cg.perc_x = (total_moment_x / total_ship_weight_kg) / ship->length * 100.0f;
        result.cg.perc_y = (total_moment_y / total_ship_weight_kg) / ship->width * 100.0f;
    }

    // --- Metacentric Height (GM) Calculation ---
    float final_kg = vertical_moment / total_ship_weight_kg;
    float displaced_volume = (total_ship_weight_kg / 1000.0f) / SEAWATER_DENSITY;
    float draft = displaced_volume / (ship->length * ship->width * WATERPLANE_COEFFICIENT);
    float kb = draft / 2.0f;
    float inertia_t = (ship->length * pow(ship->width, 3)) / 12.0f * WATERPLANE_COEFFICIENT;
    float bm = inertia_t / displaced_volume;

    result.gm = kb + bm - final_kg;
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
    printf("  - Placed / Total items: %d / %d\n", analysis.placed_item_count, ship->cargo_count);
    printf("  - Total loaded weight : %.2f t (%.1f %% of max)\n", analysis.total_cargo_weight_kg / 1000.0f, (ship->lightship_weight + analysis.total_cargo_weight_kg) / ship->max_weight * 100.0f);
    printf("  - CG (Lon / Trans)    : %.1f %% / %.1f %% | Balance: %s\n", analysis.cg.perc_x, analysis.cg.perc_y, cg_stability_str);
    printf("  - Metacentric Height (GM): %.2f m | Stability: %s\n", analysis.gm, gm_stability_str);
}