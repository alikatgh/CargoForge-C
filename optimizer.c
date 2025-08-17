/*
 * optimizer.c - Cargo placement optimization logic.
 */
#include "cargoforge.h"
#include "placement_2d.h" // Include the new placement module header

/**
 * @brief Optimizes cargo placement by delegating to the 2D placement module.
 *
 * This function acts as a high-level coordinator for the optimization phase.
 * The actual packing logic is now implemented in placement_2d.c.
 *
 * @param ship A pointer to the ship struct containing the cargo to be placed.
 */
void optimize_cargo_placement(Ship *ship) {
    // Delegate the complex placement logic to the specialized module.
    place_cargo_2d(ship);
}