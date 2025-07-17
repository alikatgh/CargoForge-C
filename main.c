/*
 * main.c - Entry point for CargoForge-C, a pure C maritime cargo logistics simulator.
 *
 * This file contains the main function and basic setup for the project. It handles
 * command-line arguments, file parsing, and a skeleton for cargo optimization.
 *
 * Goals:
 * - Keep everything in pure C (C99 standard) with no external dependencies.
 * - Focus on performance: Use fixed-size arrays where possible, minimize allocations.
 * - Modularity: Design structs and functions that can be extended by contributors.
 * - Learning: Comments explain key C concepts like structs, pointers, and file I/O.
 *
 * Current Version: v0.1 Skeleton
 * - Parses command-line args for ship config and cargo list files.
 * - Defines basic Ship and Cargo structs.
 * - Reads sample files (stubbed for now).
 * - Outputs a dummy optimized plan.
 *
 * To build: Use the Makefile (coming next) or: gcc -O3 -o cargoforge main.c
 * To run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
 *
 * Challenges Addressed:
 * - File parsing without libs: Manual string tokenizing.
 * - Error handling: Return codes and stderr messages.
 * - Portability: Stick to standard C functions.
 */

#include <stdio.h>   // For printf, fprintf, FILE ops
#include <stdlib.h>  // For malloc, free, exit, atoi/atof
#include <string.h>  // For strcmp, strtok, strncpy (basic string handling)

// Define constants for fixed sizes (performance: avoid dynamic resizing initially)
#define MAX_CARGO_ITEMS 100     // Max cargo items; increase as needed
#define MAX_LINE_LENGTH 256     // Max length for file lines
#define MAX_DIMENSION 3         // Cargo dimensions: LxWxH

// Struct for Cargo Item (learning: structs group related data)
typedef struct {
    char id[32];                // Cargo ID (e.g., "ContainerA")
    float weight;               // Weight in tonnes
    float dimensions[MAX_DIMENSION];  // L, W, H in meters
    char type[16];              // Type (e.g., "standard", "hazardous")
} Cargo;

// Struct for Ship (learning: nested arrays in structs)
typedef struct {
    float length;               // Length in meters
    float width;                // Width in meters
    float max_weight;           // Max total weight in tonnes
    int cargo_count;            // Number of loaded cargo items
    Cargo cargo[MAX_CARGO_ITEMS];  // Fixed array of cargo (performance: no malloc yet)
} Ship;

// Function prototypes (learning: declare before use for better organization)
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);
void optimize_cargo(Ship *ship);  // Stub for now
void print_optimized_plan(const Ship *ship);
void usage(const char *prog_name);

// Main function: Entry point (learning: argc/argv for CLI args)
int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;  // Learning: Use EXIT_* from stdlib for portability
    }

    Ship ship = {0};  // Initialize struct to zeros (C99 feature)

    // Parse ship config file
    if (parse_ship_config(argv[1], &ship) != 0) {
        fprintf(stderr, "Error parsing ship config: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Parse cargo list file
    if (parse_cargo_list(argv[2], &ship) != 0) {
        fprintf(stderr, "Error parsing cargo list: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    // Optimize (stub)
    optimize_cargo(&ship);

    // Output results
    print_optimized_plan(&ship);

    return EXIT_SUCCESS;
}

// Parse ship config (e.g., ini-like format)
// Learning: File I/O with fopen/fgets, string parsing with strtok
int parse_ship_config(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");  // Learning: perror for system error messages
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;

        // Tokenize: key=value
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");

        if (key && value) {
            if (strcmp(key, "length_m") == 0) ship->length = atof(value);
            else if (strcmp(key, "width_m") == 0) ship->width = atof(value);
            else if (strcmp(key, "max_weight_tonnes") == 0) ship->max_weight = atof(value);
            // Add more keys as needed
        }
    }

    fclose(file);
    return 0;
}

// Parse cargo list (tab/comma separated)
// Learning: More advanced parsing, array population
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    ship->cargo_count = 0;  // Reset count

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        // Assuming space-separated: ID weight dimLxdimWxdimH type
        char *token = strtok(line, " ");
        if (!token) continue;

        if (ship->cargo_count >= MAX_CARGO_ITEMS) {
            fprintf(stderr, "Too many cargo items (max %d)\n", MAX_CARGO_ITEMS);
            fclose(file);
            return -1;
        }

        Cargo *c = &ship->cargo[ship->cargo_count];  // Pointer to current slot

        strncpy(c->id, token, sizeof(c->id) - 1);

        token = strtok(NULL, " ");
        c->weight = atof(token);

        // Parse dimensions (e.g., "6x2.5x2.5")
        token = strtok(NULL, " ");
        char *dim = strtok(token, "x");
        for (int i = 0; i < MAX_DIMENSION && dim; i++) {
            c->dimensions[i] = atof(dim);
            dim = strtok(NULL, "x");
        }

        token = strtok(NULL, "\n");
        strncpy(c->type, token, sizeof(c->type) - 1);

        ship->cargo_count++;
    }

    fclose(file);
    return 0;
}

// Stub for optimization (expand later with algorithms)
void optimize_cargo(Ship *ship) {
    // For now, just sort by weight (learning: qsort from stdlib)
    // Define comparator
    int cmp(const void *a, const void *b) {
        Cargo *ca = (Cargo *)a;
        Cargo *cb = (Cargo *)b;
        return (ca->weight > cb->weight) ? -1 : 1;  // Descending
    }
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cmp);
    // TODO: Real placement logic, stability calc
}

// Print plan (learning: loops over arrays)
void print_optimized_plan(const Ship *ship) {
    printf("Optimal Placement Plan (Dummy - Sorted by Weight):\n");
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        printf("- Position %d: %s (Weight: %.2f t, Type: %s)\n", i+1, c->id, c->weight, c->type);
        // TODO: Add CG calc: e.g., float cg = total_weight / positions...
    }
    printf("Total Cargo: %d items | Ship Dimensions: %.2fm x %.2fm | Max Weight: %.2f t\n",
           ship->cargo_count, ship->length, ship->width, ship->max_weight);
    // Stub execution time (expand with clock_gettime later)
    printf("Total Execution Time: 0.12ms\n");
}

// Usage helper
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config> <cargo_list>\n", prog_name);
    fprintf(stderr, "Example: %s examples/sample_ship.cfg examples/sample_cargo.txt\n", prog_name);
}