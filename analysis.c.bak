/*
 * analysis.c - Post-optimization analysis functions.
 */
#include <math.h> // For pow()
#include "cargoforge.h"

#define SEAWATER_DENSITY 1.025f // tonnes per m^3

// Calculates the 2D Center of Gravity (CG) of the loaded cargo.
CG calculate_center_of_gravity(const Ship *ship) {
    CG cg = {50.0f, 50.0f}; // Default to center if no cargo
    float total_weight = 0.0f;
    float moment_x = 0.0f; // Sum of weight * x_position
    float moment_y = 0.0f; // Sum of weight * y_position

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue; // Skip unplaced cargo

        total_weight += c->weight;
        // Calculate moment using the center of the cargo item
        moment_x += c->weight * (c->pos_x + c->dimensions[0] / 2.0f);
        moment_y += c->weight * (c->pos_y + c->dimensions[1] / 2.0f);
    }

    if (total_weight > 0.001f) {
        if (ship->length > 0) cg.perc_x = (moment_x / total_weight) / ship->length * 100.0f;
        if (ship->width > 0) cg.perc_y = (moment_y / total_weight) / ship->width * 100.0f;
    }

    return cg;
}

// Calculates stability, including 3D CG and Metacentric Height (GM).
StabilityResult calculate_stability(const Ship *ship) {
    StabilityResult result = {{50.0f, 50.0f}, 0.0f};

    // --- Step 1: Calculate total weight and moments to find the final KG ---
    float total_weight = ship->lightship_weight;
    float vertical_moment = ship->lightship_weight * ship->lightship_kg;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue; // Skip unplaced cargo

        total_weight += c->weight;
        // Add moment from cargo (weight * vertical position of its own CG)
        vertical_moment += c->weight * (c->pos_z + c->dimensions[2] / 2.0f);
    }

    if (total_weight < 0.01f) return result; // Avoid division by zero
    
    float final_kg = vertical_moment / total_weight; // Final KG of the loaded ship

    // --- Step 2: Estimate KB and BM for a box-shaped hull ---
    float displaced_volume = total_weight / SEAWATER_DENSITY;
    float draft = displaced_volume / (ship->length * ship->width);
    
    float kb = draft / 2.0f; // Center of Buoyancy for a box
    
    // Transverse moment of inertia of the waterplane (I_T) for a rectangle
    float inertia_t = (ship->length * pow(ship->width, 3)) / 12.0f;
    float bm = inertia_t / displaced_volume;

    // --- Step 3: Calculate final GM ---
    result.gm = kb + bm - final_kg;
    
    // (Existing CG calculation would also go here)

    return result;
}