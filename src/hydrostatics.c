/*
 * hydrostatics.c - Hydrostatic table parsing and interpolation
 *
 * Provides real hydrostatic data from CSV tables, replacing the
 * simplified box-hull model when table data is available.
 */

#include "hydrostatics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * Linear interpolation helper: lerp between a and b by fraction t.
 */
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * Interpolate all fields of a HydroEntry between two entries.
 */
static void interpolate_entries(const HydroEntry *lo, const HydroEntry *hi,
                                float t, HydroEntry *out) {
    out->draft           = lerp(lo->draft,           hi->draft,           t);
    out->displacement    = lerp(lo->displacement,    hi->displacement,    t);
    out->km              = lerp(lo->km,              hi->km,              t);
    out->kb              = lerp(lo->kb,              hi->kb,              t);
    out->bm              = lerp(lo->bm,              hi->bm,              t);
    out->tpc             = lerp(lo->tpc,             hi->tpc,             t);
    out->mtc             = lerp(lo->mtc,             hi->mtc,             t);
    out->waterplane_area = lerp(lo->waterplane_area, hi->waterplane_area, t);
    out->lcb             = lerp(lo->lcb,             hi->lcb,             t);
}

int parse_hydro_table(const char *filename, HydroTable *table) {
    if (!filename || !table) return -1;

    memset(table, 0, sizeof(*table));

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open hydrostatic table '%s'\n", filename);
        return -1;
    }

    char line[512];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Strip \r for Windows line endings */
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
            continue;

        if (table->count >= MAX_HYDRO_ENTRIES) {
            fprintf(stderr, "Warning: Hydrostatic table exceeds %d entries, truncating\n",
                    MAX_HYDRO_ENTRIES);
            break;
        }

        HydroEntry *e = &table->entries[table->count];

        int parsed = sscanf(line, "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                            &e->draft, &e->displacement, &e->km, &e->kb,
                            &e->bm, &e->tpc, &e->mtc, &e->waterplane_area,
                            &e->lcb);

        if (parsed < 7) {
            fprintf(stderr, "Warning: Skipping malformed hydrostatic entry at line %d "
                    "(got %d fields, need at least 7)\n", line_num, parsed);
            continue;
        }

        /* Default LCB to 0 if not provided (8-column format) */
        if (parsed < 9) e->lcb = 0.0f;
        /* Default waterplane_area to 0 if not provided (7-column format) */
        if (parsed < 8) e->waterplane_area = 0.0f;

        /* Validate ascending draft order */
        if (table->count > 0 &&
            e->draft <= table->entries[table->count - 1].draft) {
            fprintf(stderr, "Error: Hydrostatic table not in ascending draft order "
                    "at line %d (%.2f <= %.2f)\n",
                    line_num, e->draft, table->entries[table->count - 1].draft);
            fclose(fp);
            return -1;
        }

        table->count++;
    }

    fclose(fp);

    if (table->count < 2) {
        fprintf(stderr, "Error: Hydrostatic table needs at least 2 entries (got %d)\n",
                table->count);
        return -1;
    }

    table->loaded = 1;
    return 0;
}

int hydro_interpolate(const HydroTable *table, float draft, HydroEntry *result) {
    if (!table || !result || table->count == 0)
        return -1;

    /* Clamp to table boundaries */
    if (draft <= table->entries[0].draft) {
        *result = table->entries[0];
        return 0;
    }
    if (draft >= table->entries[table->count - 1].draft) {
        *result = table->entries[table->count - 1];
        return 0;
    }

    /* Find bounding entries */
    for (int i = 0; i < table->count - 1; i++) {
        const HydroEntry *lo = &table->entries[i];
        const HydroEntry *hi = &table->entries[i + 1];

        if (draft >= lo->draft && draft <= hi->draft) {
            float range = hi->draft - lo->draft;
            float t = (range > 1e-6f) ? (draft - lo->draft) / range : 0.0f;
            interpolate_entries(lo, hi, t, result);
            return 0;
        }
    }

    /* Should not reach here, but return last entry as fallback */
    *result = table->entries[table->count - 1];
    return 0;
}

float hydro_draft_from_displacement(const HydroTable *table, float displacement_t) {
    if (!table || table->count == 0)
        return -1.0f;

    /* Clamp to table boundaries */
    if (displacement_t <= table->entries[0].displacement)
        return table->entries[0].draft;
    if (displacement_t >= table->entries[table->count - 1].displacement)
        return table->entries[table->count - 1].draft;

    /* Find bounding entries by displacement and inverse-interpolate */
    for (int i = 0; i < table->count - 1; i++) {
        float disp_lo = table->entries[i].displacement;
        float disp_hi = table->entries[i + 1].displacement;

        if (displacement_t >= disp_lo && displacement_t <= disp_hi) {
            float range = disp_hi - disp_lo;
            float t = (range > 1e-6f) ? (displacement_t - disp_lo) / range : 0.0f;
            return lerp(table->entries[i].draft, table->entries[i + 1].draft, t);
        }
    }

    return table->entries[table->count - 1].draft;
}
