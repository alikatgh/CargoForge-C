/*
 * longitudinal_strength.h - Shear force and bending moment calculations
 *
 * Calculates still water shear force (SWSF) and still water bending
 * moment (SWBM) along the hull by integrating the difference between
 * weight and buoyancy distributions.
 */

#ifndef LONGITUDINAL_STRENGTH_H
#define LONGITUDINAL_STRENGTH_H

#include "cargoforge.h"

/* Standard 20-interval hull division = 21 stations (AP to FP) */
#define LS_NUM_STATIONS 21

/**
 * StrengthLimits - Permissible shear force and bending moment.
 * Typically from the class society approval documentation.
 */
typedef struct StrengthLimits_ {
    float permissible_sf;       /* max allowable shear force (tonnes) */
    float permissible_bm_hog;   /* max allowable hogging BM (t-m), positive */
    float permissible_bm_sag;   /* max allowable sagging BM (t-m), positive value for the limit */
} StrengthLimits;

/**
 * LongStrengthResult - Full longitudinal strength analysis output.
 */
typedef struct {
    int   station_count;
    float station_pos[LS_NUM_STATIONS];         /* position from AP (m) */
    float weight_dist[LS_NUM_STATIONS];         /* weight per unit length (t/m) */
    float buoyancy_dist[LS_NUM_STATIONS];       /* buoyancy per unit length (t/m) */
    float net_load[LS_NUM_STATIONS];            /* weight - buoyancy (t/m) */
    float shear_force[LS_NUM_STATIONS];         /* shear force (t) */
    float bending_moment[LS_NUM_STATIONS];      /* bending moment (t-m) */

    float max_shear_force;     /* absolute max SF (t) */
    float max_sf_position;     /* position of max SF from AP (m) */
    float max_bm_hog;          /* max hogging BM (t-m), positive */
    float max_bm_sag;          /* max sagging BM (t-m), positive */
    float max_bm_position;     /* position of max |BM| from AP (m) */
} LongStrengthResult;

/**
 * calculate_longitudinal_strength - Main SWSF/SWBM calculation.
 *
 * Divides the hull into 20 intervals (21 stations from AP to FP).
 * Distributes lightship weight as a trapezoidal curve, cargo weight
 * at actual positions, and buoyancy uniformly along the waterplane.
 * Integrates to get shear force and bending moment.
 *
 * @param ship Ship with cargo placed
 * @param draft Mean draft (m)
 * @param displacement_t Total displacement (tonnes)
 * @param ship_length Ship length (m)
 * @param ship_width Ship width (m)
 */
LongStrengthResult calculate_longitudinal_strength(
    const Ship *ship, float draft, float displacement_t,
    float ship_length, float ship_width);

/**
 * check_strength_limits - Compare results against permissible limits.
 *
 * @return 1 if within limits, 0 if exceeded
 */
int check_strength_limits(const LongStrengthResult *result,
                          const StrengthLimits *limits);

#endif /* LONGITUDINAL_STRENGTH_H */
