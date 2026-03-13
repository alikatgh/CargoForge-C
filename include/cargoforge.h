/*
 * cargoforge.h - Shared types and declarations for CargoForge-C
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
/* FORWARD DECLARATIONS                                              */
/* ------------------------------------------------------------------ */

/* Forward-declared as struct tags; actual typedefs live in module headers.
 * Use struct pointers here to avoid pulling in full headers. */
struct HydroTable_;
struct TankConfig_;
struct StrengthLimits_;
struct DGInfo_;

/* ------------------------------------------------------------------ */
/* DATA STRUCTURES                                                   */
/* ------------------------------------------------------------------ */

/**
 * Cargo - A single piece of cargo.
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
    struct DGInfo_ *dg; /* NULL for non-DG cargo; set when DG: field parsed */
} Cargo;

/**
 * Ship - The vessel and its manifest.
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

    /* Optional data — NULL when not loaded (fallback to legacy behavior) */
    struct HydroTable_     *hydro;            /* Hydrostatic tables */
    struct TankConfig_     *tanks;            /* Tank configuration */
    struct StrengthLimits_ *strength_limits;  /* Permissible SF/BM limits */
} Ship;

typedef struct {
    float perc_x;   /* longitudinal CG as % of ship length */
    float perc_y;   /* transverse CG as % of ship width */
} CG;

/**
 * AnalysisResult - Complete stability analysis output.
 *
 * Includes hydrostatic parameters, trim/heel, IMO intact stability
 * criteria evaluation per MSC.267(85), free surface correction,
 * and longitudinal strength.
 */
typedef struct {
    /* Basic cargo summary */
    CG    cg;
    float total_cargo_weight_kg;
    int   placed_item_count;

    /* Hydrostatics */
    float draft;     /* mean draft (m) */
    float gm;        /* transverse metacentric height (m) */
    float kb;        /* vertical center of buoyancy (m) */
    float bm;        /* metacentric radius (m) */
    float kg;        /* vertical center of gravity (m) */

    /* Free surface correction */
    float free_surface_correction; /* virtual rise in KG (m) */
    float gm_corrected;           /* GM after free surface correction (m) */

    /* Trim and heel */
    float trim;      /* trim by stern (m) - positive = stern deeper */
    float heel;      /* heel angle (deg) - positive = starboard */
    float lcg;       /* longitudinal CG from midship (m) - positive = aft */

    /* IMO intact stability criteria (MSC.267/85 Part A, Ch 2.2) */
    float gz_at_30;      /* GZ at 30 degrees (m) */
    float gz_max;        /* maximum GZ value (m) */
    float gz_max_angle;  /* angle of maximum GZ (deg) */
    float area_0_30;     /* area under GZ curve 0-30 deg (m-rad) */
    float area_0_40;     /* area under GZ curve 0-40 deg (m-rad) */
    float area_30_40;    /* area under GZ curve 30-40 deg (m-rad) */
    int   imo_compliant; /* 1 if all IMO criteria satisfied */

    /* Longitudinal strength */
    float max_shear_force;     /* max SWSF (t) */
    float max_bending_moment;  /* max SWBM (t-m) */
    int   strength_compliant;  /* 1=pass, 0=fail, -1=not checked */

    /* Data source flags */
    int   hydro_table_used;    /* 1 if hydrostatic tables were used */
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

/* --- ship cleanup --- */
void ship_cleanup(Ship *ship);

#endif /* CARGOFORGE_H */
