/*
 * libcargoforge.h - Public API for the CargoForge maritime calculation engine
 *
 * This is the only header embedders need to include. All types are either
 * opaque (CargoForge handle) or plain C structs with stable layout.
 *
 * Thread safety: Each CargoForge handle is independent. Do not share a
 * single handle across threads without external synchronization.
 *
 * Example:
 *   CargoForge *cf;
 *   cargoforge_open(&cf);
 *   cargoforge_load_ship(cf, "ship.cfg");
 *   cargoforge_load_cargo(cf, "cargo.txt");
 *   cargoforge_optimize(cf);
 *   const CfResult *r = cargoforge_result(cf);
 *   printf("GM = %.2f m\n", r->gm_corrected);
 *   cargoforge_close(cf);
 */

#ifndef LIBCARGOFORGE_H
#define LIBCARGOFORGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* VERSION                                                            */
/* ------------------------------------------------------------------ */

#define CF_VERSION_MAJOR 3
#define CF_VERSION_MINOR 0
#define CF_VERSION_PATCH 0
#define CF_VERSION_STRING "3.0.0"

/* ------------------------------------------------------------------ */
/* ERROR CODES                                                        */
/* ------------------------------------------------------------------ */

#define CF_OK               0
#define CF_ERROR           -1   /* Generic error */
#define CF_ERR_NOMEM       -2   /* Allocation failed */
#define CF_ERR_FILE        -3   /* File not found or unreadable */
#define CF_ERR_PARSE       -4   /* Parse error in config or manifest */
#define CF_ERR_NO_SHIP     -5   /* No ship configuration loaded */
#define CF_ERR_NO_CARGO    -6   /* No cargo manifest loaded */
#define CF_ERR_OVERWEIGHT  -7   /* Total weight exceeds ship capacity */
#define CF_ERR_STATE       -8   /* Invalid operation for current state */

/* ------------------------------------------------------------------ */
/* OPAQUE HANDLE                                                      */
/* ------------------------------------------------------------------ */

typedef struct CargoForge CargoForge;

/* ------------------------------------------------------------------ */
/* RESULT STRUCTURES (stable ABI)                                     */
/* ------------------------------------------------------------------ */

/**
 * CfResult - Stability analysis output.
 * All distances in metres, angles in degrees, areas in m-rad.
 */
typedef struct {
    /* Cargo summary */
    int   placed_count;
    int   total_count;
    float cargo_weight;            /* total placed cargo weight (kg) */
    float displacement;            /* lightship + cargo + tanks (kg) */

    /* Hydrostatics */
    float draft;                   /* mean draft (m) */
    float kg;                      /* vertical center of gravity (m) */
    float kb;                      /* vertical center of buoyancy (m) */
    float bm;                      /* metacentric radius (m) */
    float gm;                      /* GM before free surface correction (m) */
    float gm_corrected;            /* GM after free surface correction (m) */
    float free_surface_correction; /* virtual rise in KG (m) */
    int   hydro_table_used;        /* 1 if hydrostatic tables were used */

    /* Trim and heel */
    float trim;                    /* trim by stern (m) */
    float heel;                    /* heel angle (deg) */
    float lcg;                     /* longitudinal CG from midship (m) */

    /* CG as percentage of ship dimensions */
    float cg_longitudinal_pct;
    float cg_transverse_pct;

    /* IMO intact stability (MSC.267/85) */
    float gz_at_30;                /* GZ at 30 degrees (m) */
    float gz_max;                  /* maximum GZ (m) */
    float gz_max_angle;            /* angle of max GZ (deg) */
    float area_0_30;               /* area under GZ 0-30 (m-rad) */
    float area_0_40;               /* area under GZ 0-40 (m-rad) */
    float area_30_40;              /* area under GZ 30-40 (m-rad) */
    int   imo_compliant;           /* 1 if all IMO criteria pass */

    /* Longitudinal strength */
    float max_shear_force;         /* max SWSF (t) */
    float max_bending_moment;      /* max SWBM (t-m) */
    int   strength_compliant;      /* 1=pass, 0=fail, -1=not checked */
} CfResult;

/**
 * CfCargoInfo - Read-only view of a single cargo item.
 */
typedef struct {
    const char *id;
    float weight;
    float length;
    float width;
    float height;
    const char *type;
    int   placed;
    float pos_x;
    float pos_y;
    float pos_z;
    /* DG info (all zero/NULL if not dangerous goods) */
    int   dg_class;
    int   dg_division;
    const char *un_number;
} CfCargoInfo;

/**
 * CfIMDGViolation - A single IMDG segregation violation.
 */
typedef struct {
    int   cargo_a;                 /* index of first cargo */
    int   cargo_b;                 /* index of second cargo (-1 for stowage) */
    int   required_segregation;    /* SegregationType enum value */
    float actual_distance;         /* actual distance (m) */
    const char *description;       /* human-readable description */
} CfIMDGViolation;

/* ------------------------------------------------------------------ */
/* LIFECYCLE                                                          */
/* ------------------------------------------------------------------ */

/**
 * Create a new CargoForge context.
 * Returns CF_OK on success, CF_ERR_NOMEM on failure.
 * The context must be freed with cargoforge_close().
 */
int cargoforge_open(CargoForge **cf);

/**
 * Destroy a CargoForge context and free all resources.
 * Safe to call with NULL.
 */
void cargoforge_close(CargoForge *cf);

/* ------------------------------------------------------------------ */
/* DATA LOADING                                                       */
/* ------------------------------------------------------------------ */

/**
 * Load ship configuration from a file path.
 * Returns CF_OK on success.
 */
int cargoforge_load_ship(CargoForge *cf, const char *config_path);

/**
 * Load cargo manifest from a file path.
 * Requires ship to be loaded first.
 * Returns CF_OK on success.
 */
int cargoforge_load_cargo(CargoForge *cf, const char *manifest_path);

/**
 * Load ship configuration from an in-memory string.
 * The string should be in the same key=value format as the config file.
 */
int cargoforge_load_ship_string(CargoForge *cf, const char *config_text);

/**
 * Load cargo manifest from an in-memory string.
 * Requires ship to be loaded first.
 */
int cargoforge_load_cargo_string(CargoForge *cf, const char *manifest_text);

/* ------------------------------------------------------------------ */
/* OPERATIONS                                                         */
/* ------------------------------------------------------------------ */

/**
 * Run 3D bin-packing optimization and stability analysis.
 * Requires both ship and cargo to be loaded.
 * After this call, results are available via cargoforge_result().
 */
int cargoforge_optimize(CargoForge *cf);

/**
 * Run stability analysis only (no placement).
 * Useful when cargo positions are already set.
 */
int cargoforge_analyze(CargoForge *cf);

/**
 * Check IMDG dangerous goods segregation compliance.
 * Requires optimization to have run first.
 */
int cargoforge_check_imdg(CargoForge *cf);

/**
 * Reset the context for reuse. Clears ship, cargo, and results.
 */
void cargoforge_reset(CargoForge *cf);

/* ------------------------------------------------------------------ */
/* RESULTS                                                            */
/* ------------------------------------------------------------------ */

/**
 * Get the analysis result. Returns NULL if optimize/analyze hasn't run.
 * The pointer is valid until the next optimize/analyze/reset/close call.
 */
const CfResult *cargoforge_result(const CargoForge *cf);

/**
 * Get the full result as a JSON string.
 * The returned pointer is valid until the next optimize/analyze/reset/close.
 * Returns NULL on error.
 */
const char *cargoforge_result_json(CargoForge *cf);

/**
 * Get cargo item info by index.
 * Returns CF_OK and fills info, or CF_ERROR if index is out of range.
 */
int cargoforge_cargo_info(const CargoForge *cf, int index, CfCargoInfo *info);

/**
 * Get the number of cargo items.
 */
int cargoforge_cargo_count(const CargoForge *cf);

/**
 * Get the number of IMDG violations (after cargoforge_check_imdg).
 * Returns -1 if IMDG check hasn't been run.
 */
int cargoforge_imdg_violation_count(const CargoForge *cf);

/**
 * Get an IMDG violation by index.
 * Returns CF_OK and fills v, or CF_ERROR if index is out of range.
 */
int cargoforge_imdg_violation(const CargoForge *cf, int index,
                              CfIMDGViolation *v);

/**
 * Check if IMDG compliant (1=yes, 0=no, -1=not checked).
 */
int cargoforge_imdg_compliant(const CargoForge *cf);

/* ------------------------------------------------------------------ */
/* ERROR HANDLING                                                     */
/* ------------------------------------------------------------------ */

/**
 * Get the last error message for this context.
 * Returns an empty string if no error.
 */
const char *cargoforge_errmsg(const CargoForge *cf);

/**
 * Get a static string description for an error code.
 */
const char *cargoforge_errstr(int code);

/* ------------------------------------------------------------------ */
/* UTILITY                                                            */
/* ------------------------------------------------------------------ */

/**
 * Get the library version string (e.g. "3.0.0").
 */
const char *cargoforge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBCARGOFORGE_H */
