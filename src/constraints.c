/*
 * constraints.c - Implementation of cargo placement constraints
 */

#include "constraints.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

int is_hazardous(const Cargo *cargo) {
    return (strcmp(cargo->type, CARGO_TYPE_HAZARDOUS) == 0);
}

int is_fragile(const Cargo *cargo) {
    return (strcmp(cargo->type, CARGO_TYPE_FRAGILE) == 0);
}

int is_reefer(const Cargo *cargo) {
    return (strcmp(cargo->type, CARGO_TYPE_REEFER) == 0);
}

float calculate_point_load(const Cargo *cargo) {
    float area = cargo->dimensions[0] * cargo->dimensions[1];
    if (area < 0.01f) return 0.0f;  // Avoid division by zero
    return (cargo->weight / 1000.0f) / area;  // tonnes/m²
}

int check_hazmat_separation(const Ship *ship, const Cargo *new_cargo,
                            float x, float y, float z) {
    if (!is_hazardous(new_cargo)) {
        return 1;  // Not hazmat, no separation needed
    }

    // Check distance to all other placed hazardous cargo
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];

        // Skip unplaced cargo and self
        if (c->pos_x < 0 || c == new_cargo) continue;

        if (is_hazardous(c)) {
            // Calculate 3D distance
            float dx = c->pos_x - x;
            float dy = c->pos_y - y;
            float dz = c->pos_z - z;
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);

            if (distance < MIN_HAZMAT_SEPARATION) {
                return 0;  // Too close to another hazmat cargo
            }
        }
    }

    return 1;  // Separation adequate
}

int check_cargo_constraints(const Ship *ship, const Cargo *cargo,
                            const Bin3D *bin, const Space3D *space) {
    // 1. Check point load constraint
    float point_load = calculate_point_load(cargo);
    if (point_load > MAX_POINT_LOAD) {
        fprintf(stderr, "Constraint violation: %s exceeds max point load (%.1f > %.1f t/m²)\n",
                cargo->id, point_load, MAX_POINT_LOAD);
        return 0;
    }

    // 2. Check hazmat separation (if hazardous)
    if (is_hazardous(cargo)) {
        if (!check_hazmat_separation(ship, cargo, space->x, space->y, space->z)) {
            fprintf(stderr, "Constraint violation: %s too close to other hazmat cargo\n",
                    cargo->id);
            return 0;
        }
    }

    // 3. Reefer cargo should be placed in specific zones (simplified: prefer deck)
    if (is_reefer(cargo)) {
        if (strcmp(bin->name, "Deck") != 0) {
            // Allow but warn - in real system would check for reefer plugs
            fprintf(stderr, "Note: Reefer cargo %s placed in %s (deck preferred)\n",
                    cargo->id, bin->name);
        }
    }

    // 4. Fragile cargo should not be at bottom if heavy items above
    // (This is simplified - full implementation would track stacking)
    if (is_fragile(cargo) && space->z < -5.0f) {
        // Deep in hold - check if this is safe for fragile items
        fprintf(stderr, "Note: Fragile cargo %s placed deep in hold\n", cargo->id);
    }

    // 5. Deck weight constraint - ensure deck doesn't exceed weight ratio
    if (strcmp(bin->name, "Deck") == 0) {
        float deck_weight_ratio = (bin->current_weight + cargo->weight) / ship->max_weight;
        if (deck_weight_ratio > MAX_DECK_WEIGHT_RATIO) {
            fprintf(stderr, "Constraint violation: Deck weight would exceed %.0f%% of max\n",
                    MAX_DECK_WEIGHT_RATIO * 100.0f);
            return 0;
        }
    }

    return 1;  // All constraints satisfied
}
