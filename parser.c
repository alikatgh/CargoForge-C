/*
 * parser.c - File parsing logic for CargoForge-C.
 */
#include <errno.h>
#include <math.h> // For NAN
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cargoforge.h"

/**
 * @brief Safely parses a string into a positive float within a given range.
 *
 * Uses strtof for robust error checking against malformed strings and overflows.
 * Prints an error and returns NAN on failure.
 *
 * @param s The string to parse.
 * @param min The minimum acceptable value.
 * @param max The maximum acceptable value.
 * @param field_name The name of the field being parsed, for error messages.
 * @return The parsed float value, or NAN on error.
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
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening ship config file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;

        float v = safe_atof(value, 0.1f, 1e9f, key);
        if (isnan(v)) {
            fclose(file);
            return -1; // Abort on invalid data
        }

        if (strcmp(key, "length_m") == 0) ship->length = v;
        else if (strcmp(key, "width_m") == 0) ship->width = v;
        else if (strcmp(key, "max_weight_tonnes") == 0) ship->max_weight = v * 1000.0f;
        else if (strcmp(key, "lightship_weight_tonnes") == 0) ship->lightship_weight = v * 1000.0f;
        else if (strcmp(key, "lightship_kg_m") == 0) ship->lightship_kg = v;
    }
    fclose(file);
    return 0;
}

// Parses a cargo list using the safe parser.
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening cargo list file");
        return -1;
    }

    // First pass: count lines to determine memory allocation size
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

    // Second pass: parse data into structs
    rewind(file);
    int line_num = 0;
    while (fgets(line, sizeof(line), file) && ship->cargo_count < ship->cargo_capacity) {
        line_num++;
        if (line[0] == '#' || line[0] == '\n') continue;

        char *saveptr;
        char *id = strtok_r(line, " \t", &saveptr);
        char *w_str = strtok_r(NULL, " \t", &saveptr);
        char *dim_str = strtok_r(NULL, " \t", &saveptr);
        char *type = strtok_r(NULL, " \t\n", &saveptr);

        if (!id || !w_str || !dim_str || !type) {
            fprintf(stderr, "Warning: Skipping malformed cargo data on line %d.\n", line_num);
            continue;
        }

        Cargo *c = &ship->cargo[ship->cargo_count];
        strncpy(c->id, id, sizeof(c->id) - 1);
        c->id[sizeof(c->id) - 1] = '\0';
        strncpy(c->type, type, sizeof(c->type) - 1);
        c->type[sizeof(c->type) - 1] = '\0';

        float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
        if (isnan(weight_t)) {
            free(ship->cargo);
            fclose(file);
            return -1;
        }
        c->weight = weight_t * 1000.0f; // tonnes -> kg

        // Parse dimensions (e.g., "12.2x2.4x2.6")
        char *dim_saveptr;
        char *tok = strtok_r(dim_str, "x", &dim_saveptr);
        bool dims_ok = true;
        for (int i = 0; i < MAX_DIMENSION; ++i) {
            if (!tok) { dims_ok = false; break; }
            float d = safe_atof(tok, 0.1f, 1e4f, "dimension");
            if (isnan(d)) { dims_ok = false; break; }
            c->dimensions[i] = d;
            tok = strtok_r(NULL, "x", &dim_saveptr);
        }

        if (!dims_ok) {
            fprintf(stderr, "Error: Incomplete or invalid dimensions for cargo '%s' on line %d\n", id, line_num);
            free(ship->cargo);
            fclose(file);
            return -1;
        }

        ship->cargo_count++;
    }
    fclose(file);
    return 0;
}