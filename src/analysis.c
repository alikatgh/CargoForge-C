/*
 * analysis.c - Stability analysis with hydrostatics, trim/heel, and IMO criteria
 *
 * Naval architecture calculations based on simplified box-hull model:
 * - Hydrostatic parameters (draft, KB, BM, KG, GM)
 * - Longitudinal trim via moment about midship
 * - Transverse heel via off-center CG
 * - GZ curve via wall-sided formula
 * - IMO intact stability criteria (MSC.267/85 Part A, Ch 2.2)
 */
#include <math.h>
#include "cargoforge.h"

/* Physical constants */
#define SEAWATER_DENSITY    1.025f  /* t/m3 at 15C */
#define BLOCK_COEFF         0.75f   /* Cb: underwater hull vol / bounding box */
#define WATERPLANE_COEFF    0.85f   /* Cw: waterplane area / L*B */
#define KB_FACTOR           0.53f   /* KB ~ 0.53*T for typical cargo ships */

/* IMO intact stability thresholds (MSC.267/85 Part A, 2.2) */
#define IMO_GM_MIN          0.15f   /* minimum GM (m) */
#define IMO_GZ_AT_30_MIN    0.20f   /* minimum GZ at 30 deg (m) */
#define IMO_GZ_MAX_ANGLE    25.0f   /* max GZ must occur >= this angle (deg) */
#define IMO_AREA_0_30_MIN   0.055f  /* area under GZ 0-30 (m-rad) */
#define IMO_AREA_0_40_MIN   0.090f  /* area under GZ 0-40 (m-rad) */
#define IMO_AREA_30_40_MIN  0.030f  /* area under GZ 30-40 (m-rad) */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Wall-sided formula for GZ at angle theta.
 * GZ(theta) = sin(theta) * [GM + BM * tan^2(theta) / 2]
 *
 * Valid for moderate angles (<~40 deg) and wall-sided hull forms.
 * Industry-standard approximation for initial stability assessment.
 */
static float gz_at_angle(float gm, float bm, float theta_deg) {
    float theta = theta_deg * (float)M_PI / 180.0f;
    float tan_theta = tanf(theta);
    return sinf(theta) * (gm + bm * tan_theta * tan_theta / 2.0f);
}

/**
 * Integrate GZ curve from angle_start to angle_end using trapezoidal rule.
 * Result in m-rad (GZ in meters, angles in radians).
 */
static float integrate_gz(float gm, float bm, float start_deg, float end_deg) {
    int steps = 100;
    float step = (end_deg - start_deg) / steps;
    float area = 0.0f;

    for (int i = 0; i < steps; i++) {
        float a1 = start_deg + i * step;
        float a2 = a1 + step;
        float gz1 = gz_at_angle(gm, bm, a1);
        float gz2 = gz_at_angle(gm, bm, a2);
        /* Only count positive GZ (restoring moment) */
        if (gz1 < 0) gz1 = 0;
        if (gz2 < 0) gz2 = 0;
        float da = step * (float)M_PI / 180.0f;
        area += (gz1 + gz2) * 0.5f * da;
    }
    return area;
}

/**
 * Find angle of maximum GZ by scanning 1-degree increments.
 */
static void find_gz_max(float gm, float bm, float *max_gz, float *max_angle) {
    *max_gz = 0.0f;
    *max_angle = 0.0f;
    for (float a = 1.0f; a <= 80.0f; a += 1.0f) {
        float gz = gz_at_angle(gm, bm, a);
        if (gz > *max_gz) {
            *max_gz = gz;
            *max_angle = a;
        }
    }
}

AnalysisResult perform_analysis(const Ship *ship) {
    AnalysisResult r;
    memset(&r, 0, sizeof(r));
    r.cg.perc_x = 50.0f;
    r.cg.perc_y = 50.0f;

    /* --- Single-pass cargo accumulation --- */
    float moment_x = 0.0f;
    float moment_y = 0.0f;
    float vertical_moment = ship->lightship_weight * ship->lightship_kg;
    float lcg_moment = 0.0f; /* moment about midship */

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue;

        r.placed_item_count++;
        r.total_cargo_weight_kg += c->weight;

        float cx = c->pos_x + c->dimensions[0] / 2.0f;
        float cy = c->pos_y + c->dimensions[1] / 2.0f;
        float cz = c->pos_z + c->dimensions[2] / 2.0f;

        moment_x += c->weight * cx;
        moment_y += c->weight * cy;
        vertical_moment += c->weight * cz;
        lcg_moment += c->weight * (cx - ship->length / 2.0f);
    }

    float displacement_kg = ship->lightship_weight + r.total_cargo_weight_kg;

    /* Overweight check */
    if (displacement_kg > ship->max_weight) {
        r.gm = NAN;
        return r;
    }

    /* --- CG as percentage of ship dimensions --- */
    if (r.total_cargo_weight_kg > 0.01f) {
        r.cg.perc_x = (moment_x / r.total_cargo_weight_kg) / ship->length * 100.0f;
        r.cg.perc_y = (moment_y / r.total_cargo_weight_kg) / ship->width * 100.0f;
    }

    /* --- Hydrostatics --- */
    float displacement_t = displacement_kg / 1000.0f;
    float displaced_vol = displacement_t / SEAWATER_DENSITY;

    r.draft = displaced_vol / (ship->length * ship->width * BLOCK_COEFF);
    r.kb = KB_FACTOR * r.draft;
    r.kg = vertical_moment / displacement_kg;

    /* BM: transverse metacentric radius = I_T / V */
    float inertia_t = (ship->length * powf(ship->width, 3) / 12.0f) * WATERPLANE_COEFF;
    r.bm = inertia_t / displaced_vol;

    r.gm = r.kb + r.bm - r.kg;

    /* --- Trim --- */
    r.lcg = lcg_moment / displacement_kg;

    /* Longitudinal BM and GM */
    float inertia_l = (ship->width * powf(ship->length, 3) / 12.0f) * WATERPLANE_COEFF;
    float bm_l = inertia_l / displaced_vol;
    float gm_l = r.kb + bm_l - r.kg;

    if (gm_l > 0.01f)
        r.trim = r.lcg * ship->length / gm_l;

    /* --- Heel --- */
    float tcg = 0.0f;
    if (r.total_cargo_weight_kg > 0.01f) {
        float avg_y = moment_y / r.total_cargo_weight_kg;
        tcg = avg_y - ship->width / 2.0f;
    }
    if (r.gm > 0.01f)
        r.heel = atanf(tcg / r.gm) * 180.0f / (float)M_PI;

    /* --- IMO GZ curve analysis --- */
    r.gz_at_30 = gz_at_angle(r.gm, r.bm, 30.0f);
    find_gz_max(r.gm, r.bm, &r.gz_max, &r.gz_max_angle);

    r.area_0_30  = integrate_gz(r.gm, r.bm, 0.0f, 30.0f);
    r.area_0_40  = integrate_gz(r.gm, r.bm, 0.0f, 40.0f);
    r.area_30_40 = integrate_gz(r.gm, r.bm, 30.0f, 40.0f);

    r.imo_compliant = (
        r.gm           >= IMO_GM_MIN &&
        r.gz_at_30     >= IMO_GZ_AT_30_MIN &&
        r.gz_max_angle >= IMO_GZ_MAX_ANGLE &&
        r.area_0_30    >= IMO_AREA_0_30_MIN &&
        r.area_0_40    >= IMO_AREA_0_40_MIN &&
        r.area_30_40   >= IMO_AREA_30_40_MIN
    ) ? 1 : 0;

    return r;
}

void print_loading_plan(const Ship *ship) {
    AnalysisResult a = perform_analysis(ship);

    printf("\n--- CargoForge Stability Analysis ---\n\n");
    printf("Ship: %.2f m x %.2f m | Max Weight: %.2f t\n",
           ship->length, ship->width, ship->max_weight / 1000.0f);
    printf("----------------------------------------------------------------------\n");

    if (isnan(a.gm)) {
        printf("  PLAN REJECTED: Total weight exceeds ship's maximum capacity.\n");
        return;
    }

    for (int i = 0; i < ship->cargo_count; ++i) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x >= 0.0f) {
            printf("  - %-15s | Pos (%7.2f, %7.2f, %6.2f) | %.2f t\n",
                   c->id, c->pos_x, c->pos_y, c->pos_z, c->weight / 1000.0f);
        }
    }

    float total_t = (ship->lightship_weight + a.total_cargo_weight_kg) / 1000.0f;

    const char *cg_str = (a.cg.perc_x >= 45 && a.cg.perc_x <= 55 &&
                           a.cg.perc_y >= 40 && a.cg.perc_y <= 60) ? "Good" : "Warning";

    const char *gm_str;
    if (a.gm < 0.3f)       gm_str = "CRITICAL - Too tender";
    else if (a.gm > 3.0f)  gm_str = "WARNING - Too stiff";
    else if (a.gm >= 0.5f && a.gm <= 2.5f) gm_str = "Optimal";
    else                    gm_str = "Acceptable";

    printf("\nLoad Summary\n");
    printf("  Placed / Total        : %d / %d\n", a.placed_item_count, ship->cargo_count);
    printf("  Displacement          : %.2f t (%.1f%% of max)\n",
           total_t, (total_t * 1000.0f / ship->max_weight) * 100.0f);
    printf("  Draft                 : %.2f m\n", a.draft);
    printf("  CG (Lon / Trans)      : %.1f%% / %.1f%% | %s\n",
           a.cg.perc_x, a.cg.perc_y, cg_str);

    printf("\nStability\n");
    printf("  KG / KB / BM          : %.2f / %.2f / %.2f m\n", a.kg, a.kb, a.bm);
    printf("  GM (transverse)       : %.2f m | %s\n", a.gm, gm_str);
    printf("  Trim (by stern)       : %.3f m\n", a.trim);
    printf("  Heel                  : %.2f deg\n", a.heel);

    printf("\nIMO Intact Stability (MSC.267/85)\n");
    printf("  GZ at 30 deg          : %.3f m  (min %.3f) %s\n",
           a.gz_at_30, (double)IMO_GZ_AT_30_MIN,
           a.gz_at_30 >= IMO_GZ_AT_30_MIN ? "OK" : "FAIL");
    printf("  Max GZ                : %.3f m at %.0f deg (min %.0f deg) %s\n",
           a.gz_max, a.gz_max_angle, (double)IMO_GZ_MAX_ANGLE,
           a.gz_max_angle >= IMO_GZ_MAX_ANGLE ? "OK" : "FAIL");
    printf("  Area 0-30 deg         : %.4f m-rad (min %.4f) %s\n",
           a.area_0_30, (double)IMO_AREA_0_30_MIN,
           a.area_0_30 >= IMO_AREA_0_30_MIN ? "OK" : "FAIL");
    printf("  Area 0-40 deg         : %.4f m-rad (min %.4f) %s\n",
           a.area_0_40, (double)IMO_AREA_0_40_MIN,
           a.area_0_40 >= IMO_AREA_0_40_MIN ? "OK" : "FAIL");
    printf("  Area 30-40 deg        : %.4f m-rad (min %.4f) %s\n",
           a.area_30_40, (double)IMO_AREA_30_40_MIN,
           a.area_30_40 >= IMO_AREA_30_40_MIN ? "OK" : "FAIL");
    printf("  Overall               : %s\n", a.imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");
}
