/*
 * constraints.h - Cargo placement constraints and validation
 *
 * Handles cargo-specific constraints:
 * - Hazardous materials separation (IMDG Code)
 * - Stacking weight limits per cargo type
 * - Refrigerated cargo zone preferences
 * - Fragile cargo protection
 * - Weight distribution limits
 */

#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

#include "cargoforge.h"
#include "placement_3d.h"

/* Cargo type classifications */
#define CARGO_TYPE_STANDARD  "standard"
#define CARGO_TYPE_HAZARDOUS "hazardous"
#define CARGO_TYPE_REEFER    "reefer"
#define CARGO_TYPE_FRAGILE   "fragile"
#define CARGO_TYPE_HEAVY     "heavy"

/* Constraint limits */
#define MIN_HAZMAT_SEPARATION   3.0f    /* minimum separation between hazmat (m) */
#define MAX_DECK_WEIGHT_RATIO   0.3f    /* max 30% of total weight on deck */
#define MAX_POINT_LOAD          1000.0f /* max point load (t/m2) */
#define MAX_STACK_PRESSURE      20.0f   /* max stacking pressure (t/m2) for standard */
#define MAX_STACK_PRESSURE_FRAGILE 5.0f /* max stacking pressure for fragile cargo */

/**
 * check_cargo_constraints - Validates if a cargo placement is safe
 *
 * Checks: point load, hazmat separation, reefer zones, fragile protection,
 * stacking weight, deck weight ratio.
 *
 * @return 1 if all constraints satisfied, 0 if any violation
 */
int check_cargo_constraints(const Ship *ship, const Cargo *cargo,
                            const Bin3D *bin, const Space3D *space);

int is_hazardous(const Cargo *cargo);
int is_fragile(const Cargo *cargo);
int is_reefer(const Cargo *cargo);

int check_hazmat_separation(const Ship *ship, const Cargo *new_cargo,
                            float x, float y, float z);

float calculate_point_load(const Cargo *cargo);

/**
 * calculate_stack_pressure - Calculate total weight pressing down at a position.
 *
 * Scans placed cargo to find items whose XY footprint overlaps the given
 * rectangle and whose Z position is above. Returns sum of weight / area in t/m2.
 */
float calculate_stack_pressure(const Ship *ship, float x, float y, float z,
                               float w, float d);

#endif /* CONSTRAINTS_H */
