/*
 * optimizer.c - Cargo placement optimization logic.
 */
#include "cargoforge.h"
#include "placement_3d.h" // 3D bin-packing module

/**
 * @brief Optimizes cargo placement using 3D bin-packing.
 *
 * This function acts as a high-level coordinator for the optimization phase.
 * Uses guillotine-based 3D bin-packing for realistic cargo placement with
 * proper volume utilization and weight distribution.
 *
 * @param ship A pointer to the ship struct containing the cargo to be placed.
 */
void optimize_cargo_placement(Ship *ship) {
    // Delegate to 3D placement algorithm
    place_cargo_3d(ship);
}