/*
 * imdg.c - IMDG Code dangerous goods segregation engine
 *
 * Implements Table 7.2.4 of the IMDG Code (Amendment 41-22).
 * The segregation matrix defines minimum separation requirements
 * between all pairs of dangerous goods classes.
 *
 * Matrix encoding:
 *   Row/column indices map to IMDG class/division as follows:
 *     0=1.1-1.6, 1=1.7(CO), 2=1.8(Desens), 3=2.1, 4=2.2, 5=2.3,
 *     6=3, 7=4.1, 8=4.2, 9=4.3, 10=5.1, 11=5.2, 12=6.1, 13=6.2,
 *     14=7, 15=8, 16=9
 *
 *   Values: 0=none, 1=away from, 2=separated from,
 *           3=separated by complete compartment, 4=separated longitudinally,
 *           5=incompatible (X)
 */

#include "imdg.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Number of entries in the segregation matrix */
#define MATRIX_SIZE 17

/**
 * IMDG Code Table 7.2.4 - Segregation matrix
 *
 * Rows/columns indexed as:
 *   [0]  Class 1.1-1.6 (Explosives, mass explosion to very insensitive)
 *   [1]  Class 1.7     (Explosives, predominantly fire hazard - CO compatible)
 *   [2]  Class 1.8     (Desensitized explosives)
 *   [3]  Class 2.1     (Flammable gases)
 *   [4]  Class 2.2     (Non-flammable, non-toxic gases)
 *   [5]  Class 2.3     (Toxic gases)
 *   [6]  Class 3       (Flammable liquids)
 *   [7]  Class 4.1     (Flammable solids)
 *   [8]  Class 4.2     (Spontaneously combustible)
 *   [9]  Class 4.3     (Dangerous when wet)
 *   [10] Class 5.1     (Oxidizing substances)
 *   [11] Class 5.2     (Organic peroxides)
 *   [12] Class 6.1     (Toxic substances)
 *   [13] Class 6.2     (Infectious substances)
 *   [14] Class 7       (Radioactive material)
 *   [15] Class 8       (Corrosive substances)
 *   [16] Class 9       (Misc dangerous substances)
 *
 * Source: IMDG Code Amendment 41-22, Table 7.2.4
 */
static const int seg_matrix[MATRIX_SIZE][MATRIX_SIZE] = {
/*             1.1-6  1.7  1.8  2.1  2.2  2.3   3   4.1  4.2  4.3  5.1  5.2  6.1  6.2   7    8    9  */
/* 1.1-6 */ {  5,     5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5  },
/* 1.7   */ {  5,     0,   0,   2,   1,   2,   2,   1,   2,   2,   2,   2,   1,   0,   2,   1,   0  },
/* 1.8   */ {  5,     0,   0,   1,   0,   1,   1,   0,   1,   1,   1,   1,   0,   0,   1,   0,   0  },
/* 2.1   */ {  5,     2,   1,   0,   0,   0,   2,   1,   2,   0,   2,   2,   0,   0,   2,   1,   0  },
/* 2.2   */ {  5,     1,   0,   0,   0,   0,   1,   0,   1,   0,   1,   1,   0,   0,   1,   0,   0  },
/* 2.3   */ {  5,     2,   1,   0,   0,   0,   2,   0,   1,   0,   2,   2,   0,   0,   2,   1,   0  },
/* 3     */ {  5,     2,   1,   2,   1,   2,   0,   0,   2,   1,   2,   2,   0,   0,   2,   1,   0  },
/* 4.1   */ {  5,     1,   0,   1,   0,   0,   0,   0,   1,   0,   1,   2,   0,   0,   1,   0,   0  },
/* 4.2   */ {  5,     2,   1,   2,   1,   1,   2,   1,   0,   1,   2,   2,   1,   0,   2,   1,   0  },
/* 4.3   */ {  5,     2,   1,   0,   0,   0,   1,   0,   1,   0,   2,   2,   0,   0,   2,   1,   0  },
/* 5.1   */ {  5,     2,   1,   2,   1,   2,   2,   1,   2,   2,   0,   2,   1,   0,   1,   2,   0  },
/* 5.2   */ {  5,     2,   1,   2,   1,   2,   2,   2,   2,   2,   2,   0,   1,   0,   2,   2,   0  },
/* 6.1   */ {  5,     1,   0,   0,   0,   0,   0,   0,   1,   0,   1,   1,   0,   0,   1,   0,   0  },
/* 6.2   */ {  5,     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0  },
/* 7     */ {  5,     2,   1,   2,   1,   2,   2,   1,   2,   2,   1,   2,   1,   0,   0,   2,   0  },
/* 8     */ {  5,     1,   0,   1,   0,   1,   1,   0,   1,   1,   2,   2,   0,   0,   2,   0,   0  },
/* 9     */ {  5,     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0  },
};

/**
 * Map an IMDG class+division to the segregation matrix index.
 */
static int class_to_index(int dg_class, int dg_division) {
    switch (dg_class) {
        case 1:
            if (dg_division >= 1 && dg_division <= 6) return 0;  /* 1.1-1.6 */
            if (dg_division == 7) return 1;  /* 1.7 (CO compatible) */
            if (dg_division == 8) return 2;  /* 1.8 (desensitized) */
            return 0;  /* default to 1.1-1.6 */
        case 2:
            if (dg_division == 1) return 3;
            if (dg_division == 2) return 4;
            if (dg_division == 3) return 5;
            return 3;  /* default to 2.1 */
        case 3: return 6;
        case 4:
            if (dg_division == 1) return 7;
            if (dg_division == 2) return 8;
            if (dg_division == 3) return 9;
            return 7;
        case 5:
            if (dg_division == 1) return 10;
            if (dg_division == 2) return 11;
            return 10;
        case 6:
            if (dg_division == 1) return 12;
            if (dg_division == 2) return 13;
            return 12;
        case 7: return 14;
        case 8: return 15;
        case 9: return 16;
        default: return -1;
    }
}

SegregationType imdg_get_segregation(int class_a, int div_a,
                                     int class_b, int div_b) {
    int idx_a = class_to_index(class_a, div_a);
    int idx_b = class_to_index(class_b, div_b);

    if (idx_a < 0 || idx_b < 0 || idx_a >= MATRIX_SIZE || idx_b >= MATRIX_SIZE)
        return SEG_NONE;

    int val = seg_matrix[idx_a][idx_b];
    if (val < 0 || val > 5) return SEG_NONE;
    return (SegregationType)val;
}

float imdg_min_distance(SegregationType seg) {
    switch (seg) {
        case SEG_NONE:               return 0.0f;
        case SEG_AWAY_FROM:          return 3.0f;
        case SEG_SEPARATED:          return 6.0f;
        case SEG_SEPARATED_COMPLETE: return 12.0f;
        case SEG_SEPARATED_LONG:     return 24.0f;
        case SEG_INCOMPATIBLE:       return -1.0f; /* cannot be on same vessel */
        default:                     return 0.0f;
    }
}

const char *imdg_segregation_name(SegregationType seg) {
    switch (seg) {
        case SEG_NONE:               return "None";
        case SEG_AWAY_FROM:          return "Away from";
        case SEG_SEPARATED:          return "Separated from";
        case SEG_SEPARATED_COMPLETE: return "Separated by complete compartment";
        case SEG_SEPARATED_LONG:     return "Separated longitudinally";
        case SEG_INCOMPATIBLE:       return "Incompatible";
        default:                     return "Unknown";
    }
}

/**
 * Calculate horizontal distance between two cargo items.
 */
static float cargo_horizontal_distance(const Cargo *a, const Cargo *b) {
    /* Use center-to-center distance minus half-extents for closest approach */
    float ax = a->pos_x + a->dimensions[0] / 2.0f;
    float ay = a->pos_y + a->dimensions[1] / 2.0f;
    float bx = b->pos_x + b->dimensions[0] / 2.0f;
    float by = b->pos_y + b->dimensions[1] / 2.0f;

    /* Edge-to-edge horizontal distance */
    float dx = fabsf(ax - bx) - (a->dimensions[0] + b->dimensions[0]) / 2.0f;
    float dy = fabsf(ay - by) - (a->dimensions[1] + b->dimensions[1]) / 2.0f;

    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;

    return sqrtf(dx * dx + dy * dy);
}

IMDGCheckResult imdg_check_all(const Ship *ship) {
    IMDGCheckResult result;
    memset(&result, 0, sizeof(result));
    result.compliant = 1;

    if (!ship) return result;

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *a = &ship->cargo[i];
        if (a->pos_x < 0 || !a->dg) continue;

        for (int j = i + 1; j < ship->cargo_count; j++) {
            const Cargo *b = &ship->cargo[j];
            if (b->pos_x < 0 || !b->dg) continue;

            SegregationType required = imdg_get_segregation(
                a->dg->dg_class, a->dg->dg_division,
                b->dg->dg_class, b->dg->dg_division);

            if (required == SEG_NONE) continue;

            float dist = cargo_horizontal_distance(a, b);
            float min_dist = imdg_min_distance(required);
            int violated = 0;

            if (required == SEG_INCOMPATIBLE) {
                violated = 1; /* Can never be on same vessel */
            } else if (dist < min_dist) {
                violated = 1;
            }

            if (violated && result.violation_count < MAX_IMDG_VIOLATIONS) {
                IMDGViolation *v = &result.violations[result.violation_count];
                v->cargo_idx_a = i;
                v->cargo_idx_b = j;
                v->required = required;
                v->actual_distance = dist;
                snprintf(v->description, sizeof(v->description),
                         "%s (Class %d.%d) vs %s (Class %d.%d): %s required, "
                         "actual distance %.1fm",
                         a->id, a->dg->dg_class, a->dg->dg_division,
                         b->id, b->dg->dg_class, b->dg->dg_division,
                         imdg_segregation_name(required), dist);
                result.violation_count++;
                result.compliant = 0;
            }
        }
    }

    return result;
}
