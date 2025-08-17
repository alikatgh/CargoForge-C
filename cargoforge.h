/*
 * cargoforge.h – Single source-of-truth for every public symbol
 */

#ifndef CARGOFORGE_H
#define CARGOFORGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* CONSTANTS                                                         */
/* ------------------------------------------------------------------ */

#define MAX_LINE_LENGTH 256
#define MAX_DIMENSION 3
#define MAX_FREE_RECTS 1024

/**
 * MAX_SHELVES – Maximum number of shelves per bin in the FFD packer.
 *
 * Adjust based on expected bin complexity; fixed-size for no allocations.
 */
#define MAX_SHELVES 100

/* ------------------------------------------------------------------ */
/* DATA STRUCTURES                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    float x;
    float y;
    float w;
    float h;
} Rect;

/**
 * Shelf – A horizontal level within a bin for side-by-side item placement.
 *
 * Purpose: Represents a fixed-height row in the 2D FFD packing heuristic.
 * Lifetime: Created and mutated inside bins during optimization.
 * Invariants: height > 0, used_width >= 0, used_width <= bin width.
 * Thread-safety: NOT thread-safe (mutated in-place).
 */
typedef struct {
    float y;          /**< Bottom y-position relative to bin (metres) */
    float height;     /**< Fixed height of this shelf (metres) */
    float used_width; /**< Used width from left (metres) */
} Shelf;

/**
 * Bin – A distinct packing area (e.g., hold or deck).
 *
 * Purpose: Enables multi-compartment packing for realistic holds/deck separation.
 * Lifetime: Stack-allocated in optimizer functions; scoped to placement.
 * Invariants: w > 0, h > 0, shelf_count <= MAX_SHELVES.
 * Thread-safety: NOT thread-safe (mutated during packing).
 */
typedef struct {
    char name[32];
    float x_offset;
    float y_offset;
    float z_offset;
    float w;
    float h;
    float used_height;
    Shelf shelves[MAX_SHELVES];
    int shelf_count;
} Bin;

/**
 * Cargo – A single piece of cargo.
 *
 * Invariants:
 * - ...
 * - pos_* are -1.0f until placed by optimizer.
 */
typedef struct {
    char  id[32];
    float weight;
    float dimensions[MAX_DIMENSION];
    char  type[16];
    float pos_x;
    float pos_y;
    float pos_z;
} Cargo;

/**
 * Ship – The vessel and its manifest.
 */
typedef struct {
    float length;
    float width;
    float max_weight;
    int   cargo_count;
    int   cargo_capacity;
    Cargo *cargo;
    float lightship_weight;
    float lightship_kg;
} Ship;

typedef struct {
    float perc_x;
    float perc_y;
} CG;

typedef struct {
    CG    cg;
    float gm;
} StabilityResult;

typedef struct {
    CG    cg;
    float gm;
    float total_cargo_weight_kg;
    int   placed_item_count;
} AnalysisResult;

/* ------------------------------------------------------------------ */
/* FUNCTION PROTOTYPES                                               */
/* ------------------------------------------------------------------ */

/* --- parser.c --- */
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);

/* --- optimizer.c --- */
void optimize_cargo_placement(Ship *ship);

/* --- analysis.c --- */
AnalysisResult perform_analysis(const Ship *ship);

/* --- main.c --- */
void print_loading_plan(const Ship *ship);
void usage(const char *prog_name);

#endif /* CARGOFORGE_H */