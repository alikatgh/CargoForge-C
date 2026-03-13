/*
 * imdg.h - IMDG Code dangerous goods segregation engine
 *
 * Implements the full IMDG Code segregation matrix (Table 7.2.4)
 * for 9 classes of dangerous goods with subdivision checking,
 * stowage category enforcement, and distance-based compliance.
 */

#ifndef IMDG_H
#define IMDG_H

#include "cargoforge.h"

/**
 * IMDG dangerous goods classes (1-9).
 * Subdivisions encoded as class * 10 + division.
 * e.g., Class 1.1 = 11, Class 2.3 = 23, Class 4.1 = 41
 */
#define IMDG_CLASSES 18  /* Number of distinct class/division entries in the matrix */

/**
 * Segregation types from IMDG Code Chapter 7.2.
 */
typedef enum {
    SEG_NONE              = 0, /* No segregation required */
    SEG_AWAY_FROM         = 1, /* "Away from": >= 3m horizontal on deck */
    SEG_SEPARATED         = 2, /* "Separated from": different compartments or >= 6m on deck */
    SEG_SEPARATED_COMPLETE = 3, /* "Separated by a complete compartment or hold from" */
    SEG_SEPARATED_LONG    = 4, /* "Separated longitudinally by an intervening complete compartment" */
    SEG_INCOMPATIBLE      = 5  /* Must not be stowed on the same vessel (X in table) */
} SegregationType;

/**
 * Stowage categories from IMDG Code Chapter 7.1.
 */
typedef enum {
    STOW_ANY       = 0, /* On deck or under deck */
    STOW_ON_DECK   = 1, /* On deck only */
    STOW_UNDER_DECK = 2  /* Under deck only */
} StowageCategory;

/**
 * DGInfo - Dangerous goods information attached to a cargo item.
 * NULL for non-DG cargo.
 */
typedef struct DGInfo_ {
    int            dg_class;     /* IMDG class (1-9) */
    int            dg_division;  /* IMDG division (0-3, 0 = no subdivision) */
    StowageCategory stowage;
    char           un_number[8]; /* UN number, e.g. "1203" */
    char           ems[16];      /* Emergency Schedule reference */
} DGInfo;

/**
 * IMDGViolation - A single segregation or stowage violation.
 */
typedef struct {
    int  cargo_idx_a;
    int  cargo_idx_b;        /* -1 for stowage violations */
    SegregationType required;
    float actual_distance;
    char description[128];
} IMDGViolation;

#define MAX_IMDG_VIOLATIONS 100

/**
 * IMDGCheckResult - Result of checking all DG cargo for IMDG compliance.
 */
typedef struct {
    int violation_count;
    IMDGViolation violations[MAX_IMDG_VIOLATIONS];
    int compliant;  /* 1 if no violations, 0 otherwise */
} IMDGCheckResult;

/**
 * imdg_get_segregation - Look up segregation type between two DG classes.
 *
 * Uses the IMDG Code Table 7.2.4 segregation matrix.
 */
SegregationType imdg_get_segregation(int class_a, int div_a,
                                     int class_b, int div_b);

/**
 * imdg_min_distance - Minimum horizontal distance for a segregation type.
 *
 * Returns required distance in metres, or -1.0 for SEG_INCOMPATIBLE.
 */
float imdg_min_distance(SegregationType seg);

/**
 * imdg_segregation_name - Human-readable name for a segregation type.
 */
const char *imdg_segregation_name(SegregationType seg);

/**
 * imdg_check_all - Check all DG cargo pairs for IMDG compliance.
 *
 * Checks segregation distances and stowage categories.
 *
 * @return result with violation count and details
 */
IMDGCheckResult imdg_check_all(const Ship *ship);

#endif /* IMDG_H */
