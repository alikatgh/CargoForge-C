/*
 * cargoforge.h – Single source-of-truth for every public symbol that
 *                  can legally be used outside the translation unit
 *                  in which it is defined.
 *
 * MAINTAINANCE NOTES
 * ------------------
 * 1.  Backward Compatibility
 *     *  NEVER change the size or layout of a struct that is already used
 *        in persisted data (e.g. save files, binary caches).  If you must
 *        extend, append new fields and bump the file-format version.
 *     *  NEVER remove or re-order enum/constant values once they have been
 *        used in a released binary.  Mark them deprecated in comments
 *        instead.
 *
 * 2.  Naming Conventions
 *     *  Public symbols use the prefix `cf_` (lowercase) or `CF_` (uppercase).
 *     *  Private symbols live in the corresponding *.c file and are declared
 *        `static`.  They MUST NOT be exposed here.
 *     *  Acronyms in names (CG, GM, etc.) are kept uppercase for clarity.
 *
 * 3.  Documentation Style
 *     *  Every public struct, constant and function gets a block comment
 *        with the following sections:
 *          - Purpose       – one-sentence “why it exists”.
 *          - Lifetime      – who allocates / who frees / who mutates.
 *          - Invariants    – numeric ranges, nullability, ownership rules.
 *          - Thread-safety – “NOT thread-safe” unless explicitly stated.
 *          - See-also      – related structs or functions.
 *
 * 4.  Units
 *     All physical quantities are SI unless otherwise noted:
 *     - length, width, height, x, y, z   → metres (m)
 *     - weight (mass)                    → kilograms (kg)
 *     - GM                               → metres (m)
 *     - angles                           → degrees (°)
 *
 * CONTRIBUTORS
 * ------------
 * When adding new functionality:
 *   1. Add the declaration here with the full documentation block.
 *   2. Implement in a *single* translation unit; do not expose helper
 *      symbols outside of it.
 *   3. Update tests in `tests/unit/` and, if necessary,
 *      `tests/integration/`.
 *   4. Update docs in `docs/api/` (we use Doxygen).
 */

#ifndef CARGOFORGE_H
#define CARGOFORGE_H

#include <stdio.h>   /* FILE, printf */
#include <stdlib.h>  /* malloc, free, exit */
#include <string.h>  /* memset, strcpy, strlen */

/* ------------------------------------------------------------------ */
/*  CONSTANTS                                                         */
/* ------------------------------------------------------------------ */

/**
 * MAX_LINE_LENGTH – Maximum number of characters (including NUL) we
 *                   expect in a single line of an input text file.
 *
 * Invariants:
 *   - Must be ≥ 2 (at least one visible char + '\0').
 *   - Used only for on-stack buffers; may be increased without ABI break.
 */
#define MAX_LINE_LENGTH 256

/**
 * MAX_DIMENSION – Number of spatial dimensions we track for each cargo
 *                 item.  Currently fixed to 3 (length, width, height).
 *
 * Changing this value affects the size of `Cargo`, therefore it is part
 * of the ABI.  Do **not** change in a stable release.
 */
#define MAX_DIMENSION 3

/**
 * MAX_FREE_RECTS – Upper bound on the number of disjoint free rectangles
 *                    kept by the 2-D bin-packing algorithm.
 *
 * Increase if you see “free-rect overflow” warnings in production logs.
 * Decrease to reduce memory footprint for embedded builds.
 * Must be ≥ 1 and a power-of-two for branchless masking tricks.
 */
#define MAX_FREE_RECTS 1024  /* Next power-of-two after 1000 */

/* ------------------------------------------------------------------ */
/*  DATA STRUCTURES                                                   */
/* ------------------------------------------------------------------ */

/**
 * Rect – A 2-D axis-aligned rectangle used to track free deck space.
 *
 * Lifetime:
 *   - Created and mutated exclusively inside `optimizer.c`.
 *   - Never heap-allocated; lives in a fixed-size stack array.
 *
 * Invariants:
 *   - w ≥ 0, h ≥ 0.
 *   - x, y are absolute coordinates in the ship’s frame.
 *   - All values are in metres.
 *
 * Thread-safety: NOT thread-safe (mutated in-place by the packer).
 */
typedef struct {
    float x; /**< Left edge, metres */
    float y; /**< Bottom edge, metres */
    float w; /**< Width,  metres */
    float h; /**< Height, metres */
} Rect;

/**
 * Cargo – A single piece of cargo.
 *
 * Lifetime:
 *   - Allocated in `parse_cargo_list()` via `realloc_array()`.
 *   - Owned by the `Ship` struct; freed in `free_ship()`.
 *
 * Invariants:
 *   - id is NUL-terminated and unique within a manifest.
 *   - weight > 0.
 *   - dimensions[i] > 0 for i ∈ {0,1,2}.
 *   - type is one of {"container", "breakbulk", "reefer", "danger"}.
 *   - pos_* are undefined until optimizer has run.
 *
 * Thread-safety: Read-only after optimization.
 */
typedef struct {
    char  id[32];                       /**< Unique cargo identifier */
    float weight;                       /**< Mass in kg */
    float dimensions[MAX_DIMENSION];    /**< L, W, H in metres */
    char  type[16];                     /**< Cargo class string */
    float pos_x;                        /**< Position on deck (metres) */
    float pos_y;                        /**< Position across deck (metres) */
    float pos_z;                        /**< Height above baseline (metres) */
} Cargo;

/**
 * Ship – The vessel and its manifest.
 *
 * Lifetime:
 *   - Allocated on the stack in `main.c`.
 *   - `cargo` array heap-allocated via `parse_cargo_list()`.
 *   - Must be released via `free_ship()` to avoid leaks.
 *
 * Invariants:
 *   - length, width, max_weight > 0.
 *   - 0 ≤ cargo_count ≤ cargo_capacity.
 *   - cargo_capacity ≥ 1.
 *   - lightship_weight is the empty-ship displacement (kg).
 *   - lightship_kg duplicates lightship_weight for backward compat.
 *
 * Thread-safety: NOT thread-safe (optimization mutates cargo positions).
 */
typedef struct {
    float length;         /**< Overall length (metres) */
    float width;          /**< Beam (metres) */
    float max_weight;     /**< Maximum allowable displacement (kg) */
    int   cargo_count;    /**< Actual number of cargos loaded */
    int   cargo_capacity; /**< Allocated length of `cargo` array */
    Cargo *cargo;         /**< Dynamically-grown array of cargos */
    float lightship_weight; /**< Empty-ship mass (kg) */
    float lightship_kg;     /**< Deprecated alias, kept for ABI */
} Ship;

/**
 * CG – Center of gravity expressed as a percentage of ship dimensions.
 *
 * Values:
 *   - perc_x ∈ [0, 100]  (0 = bow, 100 = stern)
 *   - perc_y ∈ [0, 100]  (0 = port, 100 = starboard)
 *
 * Thread-safety: Immutable value object, safe to share.
 */
typedef struct {
    float perc_x;
    float perc_y;
} CG;

/**
 * StabilityResult – Output from the naval-architecture analysis.
 *
 * Lifetime:
 *   - Returned by value from `calculate_stability()`; no ownership issues.
 *
 * Invariants:
 *   - gm ≥ 0 for positive stability.
 *   - cg is valid per `CG` invariants above.
 *
 * Thread-safety: Immutable value object, safe to share.
 */
typedef struct {
    CG    cg; /**< Relative position of the center of gravity */
    float gm; /**< Metacentric height (metres) */
} StabilityResult;

/* ------------------------------------------------------------------ */
/*  FUNCTION PROTOTYPES                                               */
/* ------------------------------------------------------------------ */

/* ------------ parser.c ------------------------------------------- */

/**
 * parse_ship_config() – Read vessel parameters from a JSON or INI file.
 *
 * @param filename  Path to configuration file.
 * @param ship      Out-parameter; must point to zeroed memory.
 * @return 0 on success, non-zero on parse or I/O error.
 *
 * Thread-safety: Must not be called concurrently on the same `Ship *`.
 */
int parse_ship_config(const char *filename, Ship *ship);

/**
 * parse_cargo_list() – Read cargo manifest.
 *
 * @param filename  Path to manifest file (CSV or JSON).
 * @param ship      Ship whose `cargo` array will be (re)allocated.
 * @return 0 on success, non-zero on parse or I/O error.
 *
 * Side-effect: Reallocates `ship->cargo`.
 * Thread-safety: Must not be called concurrently on the same `Ship *`.
 */
int parse_cargo_list(const char *filename, Ship *ship);

/* ------------ optimizer.c ---------------------------------------- */

/**
 * optimize_cargo_placement() – Run the 2-D bin-packing heuristic.
 *
 * Preconditions:
 *   - `ship->cargo_count` > 0.
 *   - All cargo dimensions and weights are > 0.
 *
 * Postconditions:
 *   - Every cargo item gets a valid (pos_x, pos_y, pos_z).
 *   - Total weight ≤ ship->max_weight.
 *
 * Thread-safety: NOT thread-safe; mutates `Ship *`.
 */
void optimize_cargo_placement(Ship *ship);

/* ------------ analysis.c ----------------------------------------- */

/**
 * calculate_center_of_gravity() – Compute CG from current cargo layout.
 *
 * @param ship  Read-only view of the loaded ship.
 * @return CG expressed as percentages.
 *
 * Thread-safety: Safe to call concurrently on const inputs.
 */
CG calculate_center_of_gravity(const Ship *ship);

/**
 * calculate_stability() – Estimate metacentric height and CG.
 *
 * @param ship  Read-only view of the loaded ship.
 * @return StabilityResult with gm ≥ 0.
 *
 * Thread-safety: Safe to call concurrently on const inputs.
 */
StabilityResult calculate_stability(const Ship *ship);

/* ------------ main.c --------------------------------------------- */

/**
 * print_loading_plan() – Human-readable report to stdout or file.
 *
 * @param ship       Finalized ship after optimization.
 * @param exec_time  Wall-clock time spent in optimizer (seconds).
 *
 * Thread-safety: Performs I/O, therefore not thread-safe.
 */
void print_loading_plan(const Ship *ship, double exec_time);

/**
 * usage() – Emit help text and exit(1).
 *
 * @param prog_name  argv[0] or override.
 *
 * Thread-safety: Performs I/O, not thread-safe.
 */
void usage(const char *prog_name);

#endif /* CARGOFORGE_H */