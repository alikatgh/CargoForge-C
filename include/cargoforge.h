/*
 * cargoforge.h – Shared types and declarations for CargoForge-C
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

/* ------------------------------------------------------------------ */
/* DATA STRUCTURES                                                   */
/* ------------------------------------------------------------------ */

/**
 * Cargo – A single piece of cargo.
 *
 * pos_* fields are -1.0f until placed by the optimizer.
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
    float total_cargo_weight_kg;
    int   placed_item_count;
} AnalysisResult;

/* ------------------------------------------------------------------ */
/* FUNCTION PROTOTYPES                                               */
/* ------------------------------------------------------------------ */

/* --- parser.c --- */
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);

/* --- analysis.c --- */
AnalysisResult perform_analysis(const Ship *ship);
void print_loading_plan(const Ship *ship);

#endif /* CARGOFORGE_H */