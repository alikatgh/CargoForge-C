/*
 * hydrostatics.h - Hydrostatic table data and interpolation
 *
 * Replaces hardcoded box-hull coefficients with real hydrostatic data
 * from CSV tables (typically exported from stability booklets).
 */

#ifndef HYDROSTATICS_H
#define HYDROSTATICS_H

#define MAX_HYDRO_ENTRIES 200

/**
 * HydroEntry - Single row of a hydrostatic table at a given draft.
 */
typedef struct {
    float draft;           /* even-keel draft (m) */
    float displacement;    /* displacement (tonnes) */
    float km;              /* height of transverse metacentre above keel (m) */
    float kb;              /* centre of buoyancy above keel (m) */
    float bm;              /* transverse metacentric radius (m) */
    float tpc;             /* tonnes per cm immersion */
    float mtc;             /* moment to change trim 1 cm (t-m) */
    float waterplane_area; /* waterplane area (m^2) */
    float lcb;             /* longitudinal centre of buoyancy from midship (m), +ve fwd */
} HydroEntry;

/**
 * HydroTable - Collection of hydrostatic entries indexed by draft.
 *
 * When loaded == 1, the analysis engine uses table interpolation instead
 * of box-hull approximations. When loaded == 0 (or pointer is NULL),
 * the legacy box-hull model is used.
 */
typedef struct HydroTable_ {
    HydroEntry entries[MAX_HYDRO_ENTRIES];
    int count;
    int loaded;
} HydroTable;

/**
 * parse_hydro_table - Parse a CSV hydrostatic table file.
 *
 * Expected CSV columns (order matters):
 *   draft_m, displacement_t, KM_m, KB_m, BM_m, TPC_t_cm, MTC_t_m, waterplane_m2, LCB_m
 *
 * Lines starting with '#' are comments. Entries must be in ascending draft order.
 *
 * @return 0 on success, -1 on error
 */
int parse_hydro_table(const char *filename, HydroTable *table);

/**
 * hydro_interpolate - Linearly interpolate hydrostatic values at a given draft.
 *
 * Clamps to table boundaries if draft is outside the range.
 *
 * @return 0 on success, -1 if table is empty
 */
int hydro_interpolate(const HydroTable *table, float draft, HydroEntry *result);

/**
 * hydro_draft_from_displacement - Reverse lookup: displacement -> draft.
 *
 * Uses linear interpolation on the displacement column.
 *
 * @return interpolated draft, or -1.0f if table is empty
 */
float hydro_draft_from_displacement(const HydroTable *table, float displacement_t);

#endif /* HYDROSTATICS_H */
