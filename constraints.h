/*
 * constraints.h - Cargo placement constraints and validation
 *
 * Handles cargo-specific constraints including:
 * - Hazardous materials separation (IMDG Code)
 * - Refrigerated cargo temperature requirements
 * - Fragile cargo stacking limits
 * - Weight distribution limits
 */

#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

#include "cargoforge.h"
#include "placement_3d.h"

// Cargo type classifications
#define CARGO_TYPE_STANDARD "standard"
#define CARGO_TYPE_HAZARDOUS "hazardous"
#define CARGO_TYPE_REEFER "reefer"
#define CARGO_TYPE_FRAGILE "fragile"
#define CARGO_TYPE_HEAVY "heavy"

// Constraint limits
#define MAX_STACK_HEIGHT_FRAGILE 2    // Maximum number of items on top of fragile cargo
#define MIN_HAZMAT_SEPARATION 3.0f    // Minimum separation between hazmat cargo (meters)
#define MAX_DECK_WEIGHT_RATIO 0.3f    // Max 30% of total weight on deck
#define MAX_POINT_LOAD 1000.0f        // Max point load per square meter (tonnes/m²)

/**
 * check_cargo_constraints - Validates if a cargo placement is safe and legal
 *
 * @param ship Ship structure
 * @param cargo Cargo item to validate
 * @param bin Target bin for placement
 * @param space Target space in bin
 *
 * @return 1 if constraints satisfied, 0 if violation
 */
int check_cargo_constraints(const Ship *ship, const Cargo *cargo,
                            const Bin3D *bin, const Space3D *space);

/**
 * is_hazardous - Check if cargo is hazardous material
 *
 * @param cargo Cargo to check
 * @return 1 if hazardous, 0 otherwise
 */
int is_hazardous(const Cargo *cargo);

/**
 * is_fragile - Check if cargo is fragile
 *
 * @param cargo Cargo to check
 * @return 1 if fragile, 0 otherwise
 */
int is_fragile(const Cargo *cargo);

/**
 * is_reefer - Check if cargo requires refrigeration
 *
 * @param cargo Cargo to check
 * @return 1 if reefer, 0 otherwise
 */
int is_reefer(const Cargo *cargo);

/**
 * check_hazmat_separation - Verify hazmat separation distance
 *
 * @param ship Ship structure with all cargo
 * @param new_cargo New hazmat cargo to place
 * @param x Target X position
 * @param y Target Y position
 * @param z Target Z position
 *
 * @return 1 if separation adequate, 0 if too close to other hazmat
 */
int check_hazmat_separation(const Ship *ship, const Cargo *new_cargo,
                            float x, float y, float z);

/**
 * calculate_point_load - Calculate load per unit area
 *
 * @param cargo Cargo item
 * @return Load in tonnes/m²
 */
float calculate_point_load(const Cargo *cargo);

#endif /* CONSTRAINTS_H */
