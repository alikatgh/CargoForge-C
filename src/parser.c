/*
 * parser.c - File parsing logic for CargoForge-C.
 *
 * Supports:
 * - Ship configuration (key=value)
 * - Cargo manifest (whitespace-delimited with optional DG: field)
 * - Hydrostatic tables (via hydrostatics.h)
 * - Tank configuration (via tanks.h)
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cargoforge.h"
#include "hydrostatics.h"
#include "tanks.h"
#include "longitudinal_strength.h"
#include "imdg.h"

/**
 * @brief Safely parses a string into a positive float within a given range.
 *
 * Uses strtof for robust error checking against malformed strings and overflows.
 * Prints an error and returns NAN on failure.
 */

 static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0; // Reset errno before the call
    float val = strtof(s, &end);

    // Check for conversion errors or out-of-range values
    if (errno != 0 || end == s || (*end != '\0' && *end != '\n') || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n", field_name, s);
        return NAN;
    }
    return val;
}

// Parses ship configuration using the safe parser.
int parse_ship_config(const char *filename, Ship *ship) {
    FILE *file;
    bool use_stdin = (strcmp(filename, "-") == 0);

    if (use_stdin) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            perror("Error opening ship config file");
            return -1;
        }
    }

    /* Temporary storage for file paths (parsed first, loaded after) */
    char hydro_path[256] = {0};
    char tanks_path[256] = {0};

    /* Temporary storage for strength limits */
    float perm_sf = 0, perm_bm_hog = 0, perm_bm_sag = 0;
    int has_strength = 0;

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;

        /* Trim leading whitespace from value */
        while (*value == ' ' || *value == '\t') value++;

        /* String-valued config keys (no numeric conversion) */
        if (strcmp(key, "hydrostatic_table") == 0) {
            strncpy(hydro_path, value, sizeof(hydro_path) - 1);
            continue;
        }
        if (strcmp(key, "tank_config") == 0) {
            strncpy(tanks_path, value, sizeof(tanks_path) - 1);
            continue;
        }

        /* Numeric-valued config keys */
        float v = safe_atof(value, 0.1f, 1e9f, key);
        if (isnan(v)) {
            if (!use_stdin) fclose(file);
            return -1; // Abort on invalid data
        }

        if (strcmp(key, "length_m") == 0) ship->length = v;
        else if (strcmp(key, "width_m") == 0) ship->width = v;
        else if (strcmp(key, "max_weight_tonnes") == 0) ship->max_weight = v * 1000.0f;
        else if (strcmp(key, "lightship_weight_tonnes") == 0) ship->lightship_weight = v * 1000.0f;
        else if (strcmp(key, "lightship_kg_m") == 0) ship->lightship_kg = v;
        else if (strcmp(key, "permissible_sf_tonnes") == 0) { perm_sf = v; has_strength = 1; }
        else if (strcmp(key, "permissible_bm_hog_t_m") == 0) { perm_bm_hog = v; has_strength = 1; }
        else if (strcmp(key, "permissible_bm_sag_t_m") == 0) { perm_bm_sag = v; has_strength = 1; }
    }

    if (!use_stdin) {
        fclose(file);
    }

    /* Load hydrostatic table if specified */
    if (hydro_path[0] != '\0') {
        ship->hydro = calloc(1, sizeof(HydroTable));
        if (ship->hydro) {
            if (parse_hydro_table(hydro_path, (HydroTable *)ship->hydro) != 0) {
                free(ship->hydro);
                ship->hydro = NULL;
                fprintf(stderr, "Warning: Failed to load hydrostatic table, using box-hull fallback\n");
            }
        }
    }

    /* Load tank configuration if specified */
    if (tanks_path[0] != '\0') {
        ship->tanks = calloc(1, sizeof(TankConfig));
        if (ship->tanks) {
            if (parse_tank_config(tanks_path, (TankConfig *)ship->tanks) != 0) {
                free(ship->tanks);
                ship->tanks = NULL;
                fprintf(stderr, "Warning: Failed to load tank config, no free surface correction\n");
            }
        }
    }

    /* Set strength limits if specified */
    if (has_strength) {
        ship->strength_limits = calloc(1, sizeof(StrengthLimits));
        if (ship->strength_limits) {
            StrengthLimits *sl = (StrengthLimits *)ship->strength_limits;
            sl->permissible_sf = perm_sf;
            sl->permissible_bm_hog = perm_bm_hog;
            sl->permissible_bm_sag = perm_bm_sag;
        }
    }

    return 0;
}

/**
 * Parse optional DG info field from cargo line.
 * Format: DG:class.division:UNnnnn:stowage:EmS:reference
 * Example: DG:3.1:UN1203:A:F-E,S-D
 *
 * Returns a heap-allocated DGInfo, or NULL if not a DG field.
 */
static DGInfo *parse_dg_field(const char *field) {
    if (!field || strncmp(field, "DG:", 3) != 0)
        return NULL;

    DGInfo *dg = calloc(1, sizeof(DGInfo));
    if (!dg) return NULL;

    /* Work on a copy */
    char buf[64];
    strncpy(buf, field + 3, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Parse class.division */
    char *saveptr;
    char *tok = strtok_r(buf, ":", &saveptr);
    if (tok) {
        /* Parse "3.1" or "3" */
        char *dot = strchr(tok, '.');
        if (dot) {
            *dot = '\0';
            dg->dg_class = atoi(tok);
            dg->dg_division = atoi(dot + 1);
        } else {
            dg->dg_class = atoi(tok);
            dg->dg_division = 0;
        }
    }

    /* UN number */
    tok = strtok_r(NULL, ":", &saveptr);
    if (tok) {
        strncpy(dg->un_number, tok, sizeof(dg->un_number) - 1);
    }

    /* Stowage category: A=any, D=on deck, U=under deck */
    tok = strtok_r(NULL, ":", &saveptr);
    if (tok) {
        if (tok[0] == 'D') dg->stowage = STOW_ON_DECK;
        else if (tok[0] == 'U') dg->stowage = STOW_UNDER_DECK;
        else dg->stowage = STOW_ANY;
    }

    /* EmS reference (rest of string) */
    tok = strtok_r(NULL, "", &saveptr);
    if (tok) {
        strncpy(dg->ems, tok, sizeof(dg->ems) - 1);
    }

    /* Validate class range */
    if (dg->dg_class < 1 || dg->dg_class > 9) {
        free(dg);
        return NULL;
    }

    return dg;
}

// Parses a cargo list using the safe parser.
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file;
    bool use_stdin = (strcmp(filename, "-") == 0);

    if (use_stdin) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            perror("Error opening cargo list file");
            return -1;
        }
    }

    // For stdin, we need to read all lines into memory first since we can't rewind
    // For files, we can still do two-pass reading
    char **lines = NULL;
    int line_count = 0;
    int line_capacity = 100; // Initial capacity

    if (use_stdin) {
        // Read all lines from stdin into memory
        lines = malloc(line_capacity * sizeof(char*));
        if (!lines) {
            fprintf(stderr, "Error: Failed to allocate memory for line buffer.\n");
            return -1;
        }

        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n') continue;

            if (line_count >= line_capacity) {
                line_capacity *= 2;
                char **new_lines = realloc(lines, line_capacity * sizeof(char*));
                if (!new_lines) {
                    fprintf(stderr, "Error: Failed to reallocate line buffer.\n");
                    for (int i = 0; i < line_count; i++) free(lines[i]);
                    free(lines);
                    return -1;
                }
                lines = new_lines;
            }

            lines[line_count] = strdup(line);
            if (!lines[line_count]) {
                fprintf(stderr, "Error: Failed to duplicate line.\n");
                for (int i = 0; i < line_count; i++) free(lines[i]);
                free(lines);
                return -1;
            }
            line_count++;
        }

        ship->cargo = malloc(line_count * sizeof(Cargo));
        if (!ship->cargo) {
            fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
            for (int i = 0; i < line_count; i++) free(lines[i]);
            free(lines);
            return -1;
        }
        ship->cargo_capacity = line_count;
        ship->cargo_count = 0;
    } else {
        // File-based: count lines first
        int count = 0;
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            if (line[0] != '#' && line[0] != '\n') count++;
        }

        ship->cargo = malloc(count * sizeof(Cargo));
        if (!ship->cargo) {
            fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
            fclose(file);
            return -1;
        }
        ship->cargo_capacity = count;
        ship->cargo_count = 0;
        rewind(file);
    }

    int line_num = 0;
    char line[MAX_LINE_LENGTH];

    for (int i = 0; ship->cargo_count < ship->cargo_capacity; i++) {
        // Read from either lines array (stdin) or file
        if (use_stdin) {
            if (i >= line_count) break;
            strncpy(line, lines[i], MAX_LINE_LENGTH - 1);
            line[MAX_LINE_LENGTH - 1] = '\0';
        } else {
            if (!fgets(line, sizeof(line), file)) break;
            if (line[0] == '#' || line[0] == '\n') continue;
        }

        line_num++;

        char *saveptr;
        char *id = strtok_r(line, " \t", &saveptr);
        char *w_str = strtok_r(NULL, " \t", &saveptr);
        char *dim_str = strtok_r(NULL, " \t", &saveptr);
        char *type = strtok_r(NULL, " \t\n", &saveptr);

        if (!id || !w_str || !dim_str || !type) {
            fprintf(stderr, "Warning: Skipping malformed cargo data on line %d.\n", line_num);
            continue;
        }

        /* Optional 5th field: DG info */
        char *dg_field = strtok_r(NULL, " \t\n", &saveptr);

        Cargo *c = &ship->cargo[ship->cargo_count];
        memset(c, 0, sizeof(*c));
        strncpy(c->id, id, sizeof(c->id) - 1);
        c->id[sizeof(c->id) - 1] = '\0';
        strncpy(c->type, type, sizeof(c->type) - 1);
        c->type[sizeof(c->type) - 1] = '\0';
        c->pos_x = -1.0f;
        c->pos_y = -1.0f;
        c->pos_z = -1.0f;
        c->dg = NULL;

        float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
        if (isnan(weight_t)) {
            free(ship->cargo);
            if (use_stdin) {
                for (int j = 0; j < line_count; j++) free(lines[j]);
                free(lines);
            } else {
                fclose(file);
            }
            return -1;
        }
        c->weight = weight_t * 1000.0f; // tonnes -> kg

        // Parse dimensions (e.g., "12.2x2.4x2.6")
        char *dim_saveptr;
        char *tok = strtok_r(dim_str, "x", &dim_saveptr);
        bool dims_ok = true;
        for (int d = 0; d < MAX_DIMENSION; ++d) {
            if (!tok) { dims_ok = false; break; }
            float dv = safe_atof(tok, 0.1f, 1e4f, "dimension");
            if (isnan(dv)) { dims_ok = false; break; }
            c->dimensions[d] = dv;
            tok = strtok_r(NULL, "x", &dim_saveptr);
        }

        if (!dims_ok) {
            fprintf(stderr, "Error: Incomplete or invalid dimensions for cargo '%s' on line %d\n", id, line_num);
            free(ship->cargo);
            if (use_stdin) {
                for (int j = 0; j < line_count; j++) free(lines[j]);
                free(lines);
            } else {
                fclose(file);
            }
            return -1;
        }

        /* Parse DG info if present */
        if (dg_field) {
            c->dg = (struct DGInfo_ *)parse_dg_field(dg_field);
        }

        ship->cargo_count++;
    }

    // Cleanup
    if (use_stdin) {
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
    } else {
        fclose(file);
    }

    return 0;
}
