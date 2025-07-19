/*
 * cargoforge.h - Shared definitions for CargoForge-C.
 *
 * Contains all shared struct definitions, constants, and function
 * prototypes used across the project modules.
 */

#ifndef CARGOFORGE_H
#define CARGOFORGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Constants ---
#define MAX_LINE_LENGTH 256     // Max length for file lines
#define MAX_DIMENSION 3         // Cargo dimensions: L, W, H
#define MAX_FREE_RECTS 1000     // Max free rectangles for packing algorithm

// --- Data Structures ---

// Represents a 2D rectangle, used for tracking free space
typedef struct {
    float x, y, w, h;
} Rect;

// Represents a single cargo item
typedef struct {
    char id[32];
    float weight;
    float dimensions[MAX_DIMENSION];
    char type[16];
    float pos_x, pos_y, pos_z;
} Cargo;

// Represents the ship and its cargo manifest
typedef struct {
    float length, width, max_weight;
    int cargo_count, cargo_capacity;
    Cargo *cargo;
    float lightship_weight;
    float lightship_kg;
} Ship;

// Represents the calculated Center of Gravity (CG)
typedef struct {
    float perc_x; // CG as a percentage along the ship's length
    float perc_y; // CG as a percentage along the ship's width
} CG;

// New struct for stability results
typedef struct {
    CG cg;
    float gm; // Metacentric Height
} StabilityResult;

// --- Function Prototypes ---

// parser.c
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);

// optimizer.c
void optimize_cargo_placement(Ship *ship);

// analysis.c
CG calculate_center_of_gravity(const Ship *ship);

StabilityResult calculate_stability(const Ship *ship);

// main.c
void print_loading_plan(const Ship *ship, double exec_time);
void usage(const char *prog_name);


#endif // CARGOFORGE_H