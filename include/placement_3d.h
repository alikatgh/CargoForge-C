/*
 * placement_3d.h - 3D bin-packing for realistic cargo placement
 *
 * Implements 3D guillotine bin-packing algorithm with:
 * - True 3D space utilization
 * - Multiple orientations per cargo item
 * - Stacking constraints
 * - Weight distribution validation
 */

#ifndef PLACEMENT_3D_H
#define PLACEMENT_3D_H

#include "cargoforge.h"

/**
 * Space3D - Represents a free 3D rectangular space in a bin
 *
 * Used by the guillotine algorithm to track available placement regions.
 */
typedef struct {
    float x, y, z;       // Bottom-left-back corner
    float width;         // X dimension
    float depth;         // Y dimension
    float height;        // Z dimension
    int is_free;         // 1 if available, 0 if occupied
} Space3D;

/**
 * Bin3D - A 3D cargo compartment with free space tracking
 *
 * Represents a hold, deck, or other cargo area with realistic 3D constraints.
 */
typedef struct {
    char name[32];
    float x, y, z;           // Origin position
    float width;             // X dimension
    float depth;             // Y dimension
    float height;            // Z dimension
    float max_weight;        // Weight capacity (kg)
    float current_weight;    // Current load (kg)
    Space3D spaces[MAX_FREE_RECTS];
    int space_count;
} Bin3D;

/**
 * place_cargo_3d - Main 3D bin-packing function
 *
 * Places cargo items into ship's compartments using 3D guillotine algorithm.
 * Sorts cargo by volume (largest first) and tries all 6 rotations per item.
 *
 * @param ship Ship structure with cargo manifest
 *
 * Side effects:
 * - Updates pos_x, pos_y, pos_z for each placed cargo item
 * - Prints warnings to stderr for unplaced items
 */
void place_cargo_3d(Ship *ship);

/**
 * find_best_fit_3d - Finds optimal bin and orientation for a cargo item
 *
 * @param bins Array of available bins
 * @param bin_count Number of bins
 * @param cargo Cargo item to place
 * @param best_bin Output: index of best bin (-1 if no fit)
 * @param best_space Output: index of best space in bin
 * @param best_orientation Output: orientation index (0-5)
 *
 * @return 1 if placement found, 0 otherwise
 */
int find_best_fit_3d(Bin3D *bins, int bin_count, const Cargo *cargo,
                     int *best_bin, int *best_space, int *best_orientation);

/**
 * split_space_3d - Splits a space after placing cargo (guillotine split)
 *
 * After placing cargo in a space, this splits the remaining volume into
 * smaller free spaces using the guillotine heuristic.
 *
 * @param bin Bin containing the space
 * @param space_idx Index of space being split
 * @param cargo Cargo that was placed
 * @param orientation Orientation used (0-5)
 */
void split_space_3d(Bin3D *bin, int space_idx, const Cargo *cargo, int orientation);

#endif /* PLACEMENT_3D_H */
