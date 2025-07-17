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
 * Current Version: v0.2 CG Calculation
 * - Parses command-line args for ship config and cargo list files.
 * - Defines basic Ship and Cargo structs.
 * - Reads sample files.
 * - Implements a Center of Gravity (CG) calculation.
 * - Outputs a dummy optimized plan with a stability check.
 *
 * To build: gcc -O3 -Wall -std=c99 -o cargoforge main.c
 * To run: ./cargoforge examples/sample_ship.cfg examples/sample_cargo.txt
 *
 * Challenges Addressed:
 * - File parsing without libs: Manual string tokenizing.
 * - Error handling: Return codes and stderr messages.
 * - Portability: Stick to standard C functions.
 */

#include <stdio.h>   // For printf, fprintf, FILE ops
#include <stdlib.h>  // For malloc, free, exit, atoi/atof
#include <string.h>  // For strcmp, strtok_r, strncpy (basic string handling)
#include <time.h>    // For clock_gettime (more precise timing)

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
    float position;             // Dummy position along ship length for CG calc (0 to ship->length)
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
void optimize_cargo(Ship *ship);
float calculate_cg(const Ship *ship); // New: Center of gravity calc
void print_optimized_plan(const Ship *ship, double exec_time);
void usage(const char *prog_name);

// Comparator for qsort (must be defined at file scope in standard C)
static int cargo_cmp(const void *a, const void *b) {
    const Cargo *ca = (const Cargo *)a;
    const Cargo *cb = (const Cargo *)b;
    // Sorts in descending order of weight
    if (ca->weight < cb->weight) return 1;
    if (ca->weight > cb->weight) return -1;
    return 0;
}

// Main function: Entry point (learning: argc/argv for CLI args)
int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Start timer for execution measurement
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    // Optimize
    optimize_cargo(&ship);

    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double exec_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;

    // Output results
    print_optimized_plan(&ship, exec_time);

    return EXIT_SUCCESS;
}

// Parse ship config (e.g., ini-like format)
int parse_ship_config(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
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
        }
    }

    fclose(file);
    return 0;
}

// Parse cargo list (tab/comma separated)
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    ship->cargo_count = 0;
    int line_num = 0;
    char *saveptr_line; // Save pointer for the main line tokenizer

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        if (line[0] == '#' || line[0] == '\n') continue;

        if (ship->cargo_count >= MAX_CARGO_ITEMS) {
            fprintf(stderr, "Warning: Too many cargo items (max %d). Ignoring rest.\n", MAX_CARGO_ITEMS);
            break;
        }

        Cargo *c = &ship->cargo[ship->cargo_count];
        c->position = 0.0f; // Initialize position

        // ID
        char *token = strtok_r(line, " \t", &saveptr_line);
        if (!token) continue;
        strncpy(c->id, token, sizeof(c->id) - 1);
        c->id[sizeof(c->id) - 1] = '\0';

        // Weight
        token = strtok_r(NULL, " \t", &saveptr_line);
        if (!token) { fprintf(stderr, "Error: Missing weight on line %d\n", line_num); continue; }
        c->weight = atof(token);

        // Dimensions
        token = strtok_r(NULL, " \t", &saveptr_line);
        if (!token) { fprintf(stderr, "Error: Missing dimensions on line %d\n", line_num); continue; }
        
        char *saveptr_dim; // Save pointer for the nested dimension tokenizer
        char *dim = strtok_r(token, "x", &saveptr_dim);
        int dim_count = 0;
        while (dim && dim_count < MAX_DIMENSION) {
            c->dimensions[dim_count++] = atof(dim);
            dim = strtok_r(NULL, "x", &saveptr_dim);
        }

        // Type
        token = strtok_r(NULL, " \t\n", &saveptr_line); // Use \n to strip trailing newline
        if (!token) { fprintf(stderr, "Error: Missing type on line %d\n", line_num); continue; }
        strncpy(c->type, token, sizeof(c->type) - 1);
        c->type[sizeof(c->type) - 1] = '\0';

        ship->cargo_count++;
    }

    fclose(file);
    return 0;
}

// Optimization Logic
void optimize_cargo(Ship *ship) {
    // 1. Sort cargo by weight (heaviest first) using qsort from stdlib.
    qsort(ship->cargo, ship->cargo_count, sizeof(Cargo), cargo_cmp);

    // 2. Assign dummy positions for CG calculation.
    // This simple model places items linearly from bow to stern.
    // Heavier items (sorted first) get placed towards the bow.
    if (ship->cargo_count > 0 && ship->length > 0) {
        float step = ship->length / ship->cargo_count;
        for (int i = 0; i < ship->cargo_count; i++) {
            // Position is the center of the allocated slot.
            ship->cargo[i].position = (i * step) + (step / 2.0f);
        }
    }
    // TODO: Implement a real 2D/3D bin packing algorithm for placement.
    // TODO: Add stability checks for roll (metacentric height).
}

// New: Calculate center of gravity (simple longitudinal CG)
float calculate_cg(const Ship *ship) {
    if (ship->cargo_count == 0 || ship->length == 0) {
        return 50.0; // Default to midship if no cargo or no length
    }

    float total_weight = 0.0;
    float weighted_moment = 0.0; // Sum of (weight * position)

    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        total_weight += c->weight;
        weighted_moment += c->weight * c->position;
    }

    if (total_weight == 0) {
        return 50.0; // Avoid division by zero
    }
    
    // CG position from the bow (in meters)
    float cg_pos_m = weighted_moment / total_weight;

    // Return as a percentage of ship's length from the bow
    return (cg_pos_m / ship->length) * 100.0;
}

// Print the final loading plan
void print_optimized_plan(const Ship *ship, double exec_time) {
    printf("--- CargoForge Optimized Plan ---\n\n");
    printf("Ship Specs:\n");
    printf("  - Dimensions: %.2fm x %.2fm\n", ship->length, ship->width);
    printf("  - Max Weight: %.2f tonnes\n\n", ship->max_weight);
    
    printf("Placement Plan (Sorted by Weight):\n");
    float total_loaded_weight = 0.0;
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        printf("  - Item %-12s | Weight: %7.2f t | Pos: %6.2fm | Type: %s\n",
               c->id, c->weight, c->position, c->type);
        total_loaded_weight += c->weight;
    }
    printf("\n");

    float cg = calculate_cg(ship);
    const char *stability = (cg >= 40.0 && cg <= 60.0) ? "Good" : "Warning - Unbalanced";

    printf("Load Summary:\n");
    printf("  - Total Cargo Items: %d\n", ship->cargo_count);
    printf("  - Total Loaded Weight: %.2f t (%.1f%% of max)\n", total_loaded_weight, (total_loaded_weight / ship->max_weight) * 100.0);
    printf("  - Longitudinal CG:   %.2f%% from bow (Stability: %s)\n\n", cg, stability);
    
    printf("----------------------------------\n");
    printf("Plan generated in %.4f ms.\n", exec_time);
}

// Usage helper
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <ship_config> <cargo_list>\n", prog_name);
    fprintf(stderr, "Example: %s examples/sample_ship.cfg examples/sample_cargo.txt\n", prog_name);
}