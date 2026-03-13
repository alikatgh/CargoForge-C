/*
 * constraints.c - Cargo placement constraints
 *
 * Enforces maritime safety rules during placement:
 * - IMDG hazmat separation (3m minimum)
 * - Point load limits
 * - Stacking weight limits (pressure per tier)
 * - Deck weight ratio
 * - Fragile cargo protection
 * - Reefer zone preferences
 */

#include "constraints.h"
#include "imdg.h"
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
    if (area < 0.01f) return 0.0f;
    return (cargo->weight / 1000.0f) / area;  /* t/m2 */
}

int check_hazmat_separation(const Ship *ship, const Cargo *new_cargo,
                            float x, float y, float z) {
    if (!is_hazardous(new_cargo))
        return 1;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0 || c == new_cargo) continue;

        if (is_hazardous(c)) {
            float dx = c->pos_x - x;
            float dy = c->pos_y - y;
            float dz = c->pos_z - z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            if (dist < MIN_HAZMAT_SEPARATION)
                return 0;
        }
    }
    return 1;
}

float calculate_stack_pressure(const Ship *ship, float x, float y, float z,
                               float w, float d) {
    float total_weight_above = 0.0f;
    float footprint = w * d;
    if (footprint < 0.01f) return 0.0f;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue;

        /* Only count cargo above this position */
        if (c->pos_z <= z) continue;

        /* Check XY overlap (AABB intersection) */
        float cx2 = c->pos_x + c->dimensions[0];
        float cy2 = c->pos_y + c->dimensions[1];
        float bx2 = x + w;
        float by2 = y + d;

        float overlap_x = fminf(cx2, bx2) - fmaxf(c->pos_x, x);
        float overlap_y = fminf(cy2, by2) - fmaxf(c->pos_y, y);

        if (overlap_x > 0 && overlap_y > 0) {
            float c_area = c->dimensions[0] * c->dimensions[1];
            if (c_area > 0.01f) {
                float overlap_frac = (overlap_x * overlap_y) / c_area;
                total_weight_above += (c->weight / 1000.0f) * overlap_frac;
            }
        }
    }

    return total_weight_above / footprint;
}

int check_cargo_constraints(const Ship *ship, const Cargo *cargo,
                            const Bin3D *bin, const Space3D *space) {

    /* 1. Point load */
    float point_load = calculate_point_load(cargo);
    if (point_load > MAX_POINT_LOAD) {
        fprintf(stderr, "Constraint: %s exceeds point load (%.1f > %.1f t/m2)\n",
                cargo->id, point_load, MAX_POINT_LOAD);
        return 0;
    }

    /* 2. Hazmat separation (IMDG-aware when DG info available) */
    if (is_hazardous(cargo) || cargo->dg) {
        if (cargo->dg) {
            /* Full IMDG segregation check */
            for (int i = 0; i < ship->cargo_count; i++) {
                const Cargo *c = &ship->cargo[i];
                if (c->pos_x < 0 || c == cargo || !c->dg) continue;

                SegregationType req = imdg_get_segregation(
                    cargo->dg->dg_class, cargo->dg->dg_division,
                    c->dg->dg_class, c->dg->dg_division);

                if (req == SEG_INCOMPATIBLE) {
                    fprintf(stderr, "Constraint: %s incompatible with %s (IMDG)\n",
                            cargo->id, c->id);
                    return 0;
                }

                float min_dist = imdg_min_distance(req);
                if (min_dist > 0.0f) {
                    float dx = c->pos_x - space->x;
                    float dy = c->pos_y - space->y;
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist < min_dist) {
                        fprintf(stderr, "Constraint: %s too close to %s (IMDG %s: %.1fm < %.1fm)\n",
                                cargo->id, c->id, imdg_segregation_name(req), dist, min_dist);
                        return 0;
                    }
                }
            }
        } else {
            /* Legacy 3m hazmat separation fallback */
            if (!check_hazmat_separation(ship, cargo, space->x, space->y, space->z)) {
                fprintf(stderr, "Constraint: %s too close to other hazmat cargo\n", cargo->id);
                return 0;
            }
        }
    }

    /* 3. Stacking weight limits */
    float stack_pressure = calculate_stack_pressure(
        ship, space->x, space->y, space->z,
        cargo->dimensions[0], cargo->dimensions[1]);

    float max_pressure = is_fragile(cargo) ? MAX_STACK_PRESSURE_FRAGILE : MAX_STACK_PRESSURE;
    if (stack_pressure > max_pressure) {
        fprintf(stderr, "Constraint: %s stacking pressure %.1f t/m2 exceeds limit %.1f\n",
                cargo->id, stack_pressure, max_pressure);
        return 0;
    }

    /* 4. Reefer cargo: prefer deck */
    if (is_reefer(cargo) && strcmp(bin->name, "Deck") != 0) {
        fprintf(stderr, "Note: Reefer %s placed in %s (deck preferred)\n",
                cargo->id, bin->name);
    }

    /* 5. Fragile cargo: warn if deep in hold */
    if (is_fragile(cargo) && space->z < -5.0f) {
        fprintf(stderr, "Note: Fragile %s placed deep in hold (z=%.1f)\n",
                cargo->id, space->z);
    }

    /* 6. Deck weight ratio */
    if (strcmp(bin->name, "Deck") == 0) {
        float deck_weight_ratio = (bin->current_weight + cargo->weight) / ship->max_weight;
        if (deck_weight_ratio > MAX_DECK_WEIGHT_RATIO) {
            fprintf(stderr, "Constraint: Deck weight exceeds %.0f%% limit\n",
                    MAX_DECK_WEIGHT_RATIO * 100.0f);
            return 0;
        }
    }

    return 1;
}
