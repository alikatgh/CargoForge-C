/*
 * analysis.c - Post-optimization analysis functions.
 */
#include "cargoforge.h"

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