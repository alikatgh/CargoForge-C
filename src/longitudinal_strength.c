/*
 * longitudinal_strength.c - SWSF and SWBM calculations
 *
 * Implements still water shear force and bending moment analysis
 * using standard 20-interval hull station division.
 */

#include "longitudinal_strength.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/**
 * Distribute lightship weight as a trapezoidal curve along the hull.
 *
 * A typical cargo ship has more structure amidships than at the ends.
 * We approximate this with a trapezoidal distribution: weight per unit
 * length is lower at AP and FP, peaking in the middle 60%.
 *
 * The distribution integrates to total lightship weight.
 */
static void distribute_lightship(float *dist, const float *stations,
                                 int count, float lightship_t, float length) {
    /*
     * Trapezoidal shape:
     *   0-20% length: linearly ramp from 0.6*avg to 1.0*avg
     *   20-80% length: constant at 1.15*avg
     *   80-100% length: linearly ramp from 1.0*avg to 0.5*avg
     *
     * These factors are chosen so the integral equals lightship_t.
     */
    float avg_wt = lightship_t / length; /* average weight per metre */

    /* First pass: compute raw distribution */
    float raw_total = 0.0f;
    float raw[LS_NUM_STATIONS];

    for (int i = 0; i < count; i++) {
        float frac = stations[i] / length; /* 0 at AP, 1 at FP */

        if (frac < 0.2f) {
            /* Ramp up from stern */
            float t = frac / 0.2f;
            raw[i] = avg_wt * (0.6f + 0.4f * t);
        } else if (frac < 0.8f) {
            /* Midship region */
            raw[i] = avg_wt * 1.15f;
        } else {
            /* Ramp down to bow */
            float t = (frac - 0.8f) / 0.2f;
            raw[i] = avg_wt * (1.0f - 0.5f * t);
        }
        raw_total += raw[i];
    }

    /* Normalize so total matches lightship weight */
    float scale = (raw_total > 1e-6f) ? (lightship_t / length) / (raw_total / count) : 0.0f;
    /* More precise: use trapezoidal integration */
    float integrated = 0.0f;
    for (int i = 0; i < count - 1; i++) {
        float dx = stations[i + 1] - stations[i];
        integrated += (raw[i] + raw[i + 1]) * 0.5f * dx;
    }
    scale = (integrated > 1e-6f) ? lightship_t / integrated : 0.0f;

    for (int i = 0; i < count; i++) {
        dist[i] = raw[i] * scale;
    }
}

/**
 * Add cargo weight to the station-based distribution.
 *
 * Each cargo item's weight is distributed across the stations it spans.
 * For items smaller than one station interval, weight is split between
 * the two nearest stations.
 */
static void distribute_cargo(float *dist, const float *stations, int count,
                             const Ship *ship) {
    float dx = stations[1] - stations[0]; /* station spacing */

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue; /* not placed */

        float cargo_t = c->weight / 1000.0f;
        float x_start = c->pos_x;
        float x_end = c->pos_x + c->dimensions[0];
        float cargo_length = x_end - x_start;

        if (cargo_length < 0.01f) continue;

        float wt_per_m = cargo_t / cargo_length;

        /* Distribute across overlapping stations */
        for (int s = 0; s < count; s++) {
            float s_lo = stations[s] - dx / 2.0f;
            float s_hi = stations[s] + dx / 2.0f;
            if (s == 0) s_lo = 0.0f;
            if (s == count - 1) s_hi = stations[count - 1];

            float overlap_lo = fmaxf(s_lo, x_start);
            float overlap_hi = fminf(s_hi, x_end);

            if (overlap_hi > overlap_lo) {
                float overlap_len = overlap_hi - overlap_lo;
                dist[s] += wt_per_m * overlap_len / dx;
            }
        }
    }
}

/**
 * Distribute buoyancy uniformly along the waterplane.
 *
 * For a box-hull approximation, buoyancy per unit length is:
 *   b(x) = rho * B * T * Cb (constant)
 *
 * The total buoyancy equals displacement.
 */
static void distribute_buoyancy(float *dist, const float *stations, int count,
                                float displacement_t, float length) {
    float buoy_per_m = displacement_t / length;

    for (int i = 0; i < count; i++) {
        dist[i] = buoy_per_m;
    }

    /* Refine: taper at ends (bow/stern have less buoyancy) */
    for (int i = 0; i < count; i++) {
        float frac = stations[i] / length;
        float taper = 1.0f;

        if (frac < 0.05f) {
            taper = 0.5f + (frac / 0.05f) * 0.5f;
        } else if (frac > 0.95f) {
            taper = 0.5f + ((1.0f - frac) / 0.05f) * 0.5f;
        }
        dist[i] *= taper;
    }

    /* Normalize to match total displacement */
    float integrated = 0.0f;
    for (int i = 0; i < count - 1; i++) {
        float dx = stations[i + 1] - stations[i];
        integrated += (dist[i] + dist[i + 1]) * 0.5f * dx;
    }
    if (integrated > 1e-6f) {
        float scale = displacement_t / integrated;
        for (int i = 0; i < count; i++) {
            dist[i] *= scale;
        }
    }
}

LongStrengthResult calculate_longitudinal_strength(
    const Ship *ship, float draft, float displacement_t,
    float ship_length, float ship_width) {

    LongStrengthResult r;
    memset(&r, 0, sizeof(r));
    r.station_count = LS_NUM_STATIONS;

    (void)draft;
    (void)ship_width;

    /* Set up station positions (evenly spaced, AP to FP) */
    float dx = ship_length / (float)(LS_NUM_STATIONS - 1);
    for (int i = 0; i < LS_NUM_STATIONS; i++) {
        r.station_pos[i] = i * dx;
    }

    /* Build weight distribution */
    float lightship_t = ship->lightship_weight / 1000.0f;
    distribute_lightship(r.weight_dist, r.station_pos, LS_NUM_STATIONS,
                         lightship_t, ship_length);
    distribute_cargo(r.weight_dist, r.station_pos, LS_NUM_STATIONS, ship);

    /* Build buoyancy distribution */
    distribute_buoyancy(r.buoyancy_dist, r.station_pos, LS_NUM_STATIONS,
                        displacement_t, ship_length);

    /* Net load = weight - buoyancy (positive = excess weight) */
    for (int i = 0; i < LS_NUM_STATIONS; i++) {
        r.net_load[i] = r.weight_dist[i] - r.buoyancy_dist[i];
    }

    /* Integrate net load to get shear force (trapezoidal rule) */
    r.shear_force[0] = 0.0f;
    for (int i = 1; i < LS_NUM_STATIONS; i++) {
        r.shear_force[i] = r.shear_force[i - 1] +
            (r.net_load[i - 1] + r.net_load[i]) * 0.5f * dx;
    }

    /* Integrate shear force to get bending moment */
    r.bending_moment[0] = 0.0f;
    for (int i = 1; i < LS_NUM_STATIONS; i++) {
        r.bending_moment[i] = r.bending_moment[i - 1] +
            (r.shear_force[i - 1] + r.shear_force[i]) * 0.5f * dx;
    }

    /* Find maxima */
    r.max_shear_force = 0.0f;
    r.max_bm_hog = 0.0f;
    r.max_bm_sag = 0.0f;

    for (int i = 0; i < LS_NUM_STATIONS; i++) {
        float abs_sf = fabsf(r.shear_force[i]);
        if (abs_sf > r.max_shear_force) {
            r.max_shear_force = abs_sf;
            r.max_sf_position = r.station_pos[i];
        }

        /* Hogging = positive BM (deck in tension), Sagging = negative BM */
        if (r.bending_moment[i] > r.max_bm_hog) {
            r.max_bm_hog = r.bending_moment[i];
            r.max_bm_position = r.station_pos[i];
        }
        if (r.bending_moment[i] < 0 && fabsf(r.bending_moment[i]) > r.max_bm_sag) {
            r.max_bm_sag = fabsf(r.bending_moment[i]);
            if (fabsf(r.bending_moment[i]) > r.max_bm_hog) {
                r.max_bm_position = r.station_pos[i];
            }
        }
    }

    return r;
}

int check_strength_limits(const LongStrengthResult *result,
                          const StrengthLimits *limits) {
    if (!result || !limits) return -1;

    if (result->max_shear_force > limits->permissible_sf)
        return 0;
    if (result->max_bm_hog > limits->permissible_bm_hog)
        return 0;
    if (result->max_bm_sag > limits->permissible_bm_sag)
        return 0;

    return 1;
}
