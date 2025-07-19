/*
 * parser.c - File parsing logic for CargoForge-C.
 */
#include "cargoforge.h"

// Parses ship configuration from a simple key=value file.
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

        if (key && value) {
            if (strcmp(key, "length_m") == 0) ship->length = atof(value);
            else if (strcmp(key, "width_m") == 0) ship->width = atof(value);
            else if (strcmp(key, "max_weight_tonnes") == 0) ship->max_weight = atof(value);
            else if (strcmp(key, "lightship_weight_tonnes") == 0) ship->lightship_weight = atof(value);
            else if (strcmp(key, "lightship_kg_m") == 0) ship->lightship_kg = atof(value);
        }
    }
    fclose(file);
    return 0;
}


// Parses a cargo list, dynamically allocating memory for the items.
// Returns 0 on success, -1 on failure.
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening cargo list file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;

    // First pass: count the number of valid cargo entries to allocate memory
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '#' && line[0] != '\n') {
            count++;
        }
    }

    // Allocate exact memory required
    ship->cargo = malloc(count * sizeof(Cargo));
    if (!ship->cargo) {
        fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
        fclose(file);
        return -1;
    }
    ship->cargo_capacity = count;
    ship->cargo_count = 0;

    // Second pass: rewind and parse data into the allocated structs
    rewind(file);
    int line_num = 0;
    while (fgets(line, sizeof(line), file) && ship->cargo_count < ship->cargo_capacity) {
        line_num++;
        if (line[0] == '#' || line[0] == '\n') continue;

        Cargo *c = &ship->cargo[ship->cargo_count];
        char *saveptr;

        // Tokenize the line: ID Weight L_x_W_x_H Type
        char *id = strtok_r(line, " \t", &saveptr);
        char *weight = strtok_r(NULL, " \t", &saveptr);
        char *dims = strtok_r(NULL, " \t", &saveptr);
        char *type = strtok_r(NULL, " \t\n", &saveptr);

        if (!id || !weight || !dims || !type) {
            fprintf(stderr, "Warning: Skipping malformed cargo data on line %d.\n", line_num);
            continue; // Skip this line
        }

        // Populate the struct
        strncpy(c->id, id, sizeof(c->id) - 1);
        c->id[sizeof(c->id) - 1] = '\0';
        c->weight = atof(weight);
        strncpy(c->type, type, sizeof(c->type) - 1);
        c->type[sizeof(c->type) - 1] = '\0';

        // Parse dimensions (e.g., "12.2x2.4x2.6")
        char *dim_saveptr;
        char *dim_token = strtok_r(dims, "x", &dim_saveptr);
        int i = 0;
        while (dim_token && i < MAX_DIMENSION) {
            c->dimensions[i++] = atof(dim_token);
            dim_token = strtok_r(NULL, "x", &dim_saveptr);
        }

        ship->cargo_count++;
    }

    fclose(file);
    return 0;
}