/*
 * analysis.c - Post-optimization analysis and reporting.
 *
 * This file contains the logic to analyze the final cargo layout,
 * calculating stability metrics and printing the final report.
 */
#include <math.h>    // For pow() and NAN
#include <stdbool.h> // For bool type
#include "cargoforge.h"

// Physical and regulatory constants for naval architecture
#define SEAWATER_DENSITY 1.025f  // tonnes per cubic metre at 15°C
#define BLOCK_COEFFICIENT 0.75f  // Cb: ratio of underwater hull volume to bounding box (typical cargo ship)
#define WATERPLANE_COEFFICIENT 0.85f  // Cw: ratio of waterplane area to L×B rectangle
#define KB_FACTOR 0.53f          // KB ≈ 0.53 × T for typical cargo ships (vertical CG of buoyancy)
#define MIDSHIP_COEFFICIENT 0.98f // Cm: fullness of midship section

/**
 * @brief Performs all post-placement calculations in a single, efficient pass.
 *
 * This function iterates through the cargo list only once to calculate all
 * necessary metrics for the final report, including 2D/3D centers of
 * gravity, total weights, and metacentric height.
 *
 * @param ship A read-only pointer to the fully loaded Ship struct.
 * @return An AnalysisResult struct containing all calculated data.
 */
AnalysisResult perform_analysis(const Ship *ship) {
    AnalysisResult result = {{50.0f, 50.0f}, 0.0f, 0.0f, 0};

    // --- Single Pass Calculation ---
    float moment_x = 0.0f;
    float moment_y = 0.0f;
    float vertical_moment = ship->lightship_weight * ship->lightship_kg;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue; // Skip unplaced cargo

        result.placed_item_count++;
        result.total_cargo_weight_kg += c->weight;
        moment_x += c->weight * (c->pos_x + c->dimensions[0] / 2.0f);
        moment_y += c->weight * (c->pos_y + c->dimensions[1] / 2.0f);
        vertical_moment += c->weight * (c->pos_z + c->dimensions[2] / 2.0f);
    }
    
    float total_ship_weight_kg = ship->lightship_weight + result.total_cargo_weight_kg;

    // Check for overweight condition
    if (total_ship_weight_kg > ship->max_weight) {
        result.gm = NAN;
        return result;
    }
    
    // Calculate final CG and GM
    if (result.total_cargo_weight_kg > 0.01f) {
        result.cg.perc_x = (moment_x / result.total_cargo_weight_kg) / ship->length * 100.0f;
        result.cg.perc_y = (moment_y / result.total_cargo_weight_kg) / ship->width * 100.0f;
    }

    // KG: Vertical center of gravity from keel
    float final_kg = vertical_moment / total_ship_weight_kg;

    // Calculate realistic draft using block coefficient
    // Volume = Displacement / density
    // Volume = L × B × T × Cb (block coefficient accounts for hull shape)
    float displaced_volume = (total_ship_weight_kg / 1000.0f) / SEAWATER_DENSITY;
    float draft = displaced_volume / (ship->length * ship->width * BLOCK_COEFFICIENT);

    // KB: Vertical center of buoyancy from keel (typically 0.53-0.67 of draft for cargo ships)
    float kb = KB_FACTOR * draft;

    // BM: Transverse metacentric radius (distance from B to M)
    // IT = second moment of area of waterplane about centerline
    // For rectangular waterplane: IT = L × B³ / 12
    // Apply waterplane coefficient for realistic hull shape
    float inertia_t = (ship->length * pow(ship->width, 3) / 12.0f) * WATERPLANE_COEFFICIENT;
    float bm = inertia_t / displaced_volume;

    // GM: Metacentric height (positive = stable, negative = unstable)
    // Typical range for cargo ships: 0.5m - 2.5m
    // Too high (>3m) = stiff/uncomfortable rolling
    // Too low (<0.3m) = tender/unstable
    result.gm = kb + bm - final_kg;
    return result;
}


/**
 * @brief Prints the final, formatted loading plan and analysis report.
 *
 * This function is responsible only for presentation. It calls perform_analysis
 * to get the data and then formats it for human-readable output.
 *
 * @param ship A read-only pointer to the fully loaded Ship struct.
 */
void print_loading_plan(const Ship *ship) {
    AnalysisResult analysis = perform_analysis(ship);

    printf("\n--- CargoForge Stability Analysis ---\n\n");
    printf("Ship Specs: %.2f m × %.2f m | Max Weight: %.2f t\n",
           ship->length, ship->width, ship->max_weight / 1000.0f);
    printf("----------------------------------------------------------------------\n");

    if (isnan(analysis.gm)) {
        printf("  - 🔴 PLAN REJECTED: Total weight exceeds ship's maximum capacity.\n");
        return;
    }

    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x >= 0.0f) {
            printf("  - %-15s | Pos (X,Y): (%7.2f, %7.2f) | %.2f t\n",
                   c->id, c->pos_x, c->pos_y, c->weight / 1000.0f);
        }
    }

    // Maritime safety margins
    const float DWT_SAFETY_FACTOR = 0.90f;  // 90% utilization recommended
    const float usable_capacity_kg = ship->max_weight * DWT_SAFETY_FACTOR;

    // CG should be near midship (45-55% longitudinal, 40-60% transverse)
    const char *cg_stability_str = (analysis.cg.perc_x >= 45 && analysis.cg.perc_x <= 55 &&
                                    analysis.cg.perc_y >= 40 && analysis.cg.perc_y <= 60) ? "Good" : "Warning";

    // GM stability ranges for cargo ships (IMO guidelines):
    // GM < 0.3m: Unstable (dangerous)
    // GM 0.5-2.5m: Optimal (safe and comfortable)
    // GM > 3.0m: Over-stiff (excessive rolling acceleration)
    const char *gm_stability_str;
    if (analysis.gm < 0.3f) {
        gm_stability_str = "CRITICAL - Too tender";
    } else if (analysis.gm > 3.0f) {
        gm_stability_str = "WARNING - Too stiff";
    } else if (analysis.gm >= 0.5f && analysis.gm <= 2.5f) {
        gm_stability_str = "Optimal";
    } else {
        gm_stability_str = "Acceptable";
    }
    
    float total_ship_weight_kg = ship->lightship_weight + analysis.total_cargo_weight_kg;

    printf("\nLoad Summary\n");
    printf("  - Placed / Total items: %d / %d\n", analysis.placed_item_count, ship->cargo_count);
    printf("  - Total loaded weight : %.2f t (%.1f %% of max)\n",
           analysis.total_cargo_weight_kg / 1000.0f,
           (total_ship_weight_kg / ship->max_weight) * 100.0f);
           
    if (total_ship_weight_kg > usable_capacity_kg) {
        printf("  - 🔴 WARNING: exceeds %.0f %% DWT safety margin!\n",
               DWT_SAFETY_FACTOR * 100.0f);
    }
    printf("  - CG (Lon / Trans)    : %.1f %% / %.1f %% | Balance: %s\n",
           analysis.cg.perc_x, analysis.cg.perc_y, cg_stability_str);
    printf("  - Metacentric Height (GM): %.2f m | Stability: %s\n",
           analysis.gm, gm_stability_str);
}