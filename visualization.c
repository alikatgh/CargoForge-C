/*
 * visualization.c - ASCII visualization implementation
 */

#include "visualization.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define GRID_WIDTH 80
#define GRID_HEIGHT 20

void print_cargo_layout_ascii(const Ship *ship) {
    if (!ship || ship->cargo_count == 0) {
        printf("\n[No cargo to visualize]\n");
        return;
    }

    // Create a character grid
    char grid[GRID_HEIGHT][GRID_WIDTH + 1];
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = '.';
        }
        grid[y][GRID_WIDTH] = '\0';
    }

    // Scale factors to fit ship onto grid
    float scale_x = GRID_WIDTH / ship->length;
    float scale_y = GRID_HEIGHT / ship->width;

    // Draw cargo items
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        if (c->pos_x < 0) continue;  // Skip unplaced cargo

        // Map cargo position to grid coordinates
        int grid_x = (int)(c->pos_x * scale_x);
        int grid_y = (int)(c->pos_y * scale_y);
        int grid_w = (int)ceil(c->dimensions[0] * scale_x);
        int grid_h = (int)ceil(c->dimensions[1] * scale_y);

        // Ensure we have at least 1x1
        if (grid_w < 1) grid_w = 1;
        if (grid_h < 1) grid_h = 1;

        // Draw cargo on grid
        for (int dy = 0; dy < grid_h && (grid_y + dy) < GRID_HEIGHT; dy++) {
            for (int dx = 0; dx < grid_w && (grid_x + dx) < GRID_WIDTH; dx++) {
                grid[grid_y + dy][grid_x + dx] = '#';
            }
        }
    }

    // Print the grid
    printf("\n=== Top-Down Cargo Layout (ASCII) ===\n");
    printf("Ship: %.1fm (L) x %.1fm (W)\n", ship->length, ship->width);
    printf("Scale: # = cargo, . = empty\n\n");

    // Top border
    printf("   +");
    for (int x = 0; x < GRID_WIDTH; x++) printf("-");
    printf("+\n");

    // Grid with side borders
    for (int y = 0; y < GRID_HEIGHT; y++) {
        printf("%2d |%s|\n", y, grid[y]);
    }

    // Bottom border
    printf("   +");
    for (int x = 0; x < GRID_WIDTH; x++) printf("-");
    printf("+\n");

    // X-axis labels
    printf("    0");
    for (int x = 10; x < GRID_WIDTH; x += 10) {
        printf("%9d", x);
    }
    printf("\n\n");
}

void print_cargo_summary(const Ship *ship) {
    printf("\n=== Cargo Placement Summary ===\n\n");
    printf("%-15s | %-10s | %8s | %8s | %8s | %-10s\n",
           "Cargo ID", "Type", "Weight", "Position", "Dims", "Status");
    printf("----------------+------------+----------+----------+----------+------------\n");

    int placed = 0;
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];
        char pos_str[20];
        const char *status;

        if (c->pos_x >= 0) {
            snprintf(pos_str, sizeof(pos_str), "%.1f,%.1f,%.1f",
                     c->pos_x, c->pos_y, c->pos_z);
            status = "Placed";
            placed++;
        } else {
            snprintf(pos_str, sizeof(pos_str), "-");
            status = "UNPLACED";
        }

        printf("%-15s | %-10s | %7.1ft | %8s | %.1fx%.1fx%.1f | %-10s\n",
               c->id,
               c->type,
               c->weight / 1000.0f,  // Convert to tonnes
               pos_str,
               c->dimensions[0], c->dimensions[1], c->dimensions[2],
               status);
    }

    printf("\nPlacement rate: %d/%d items (%.1f%%)\n\n",
           placed, ship->cargo_count,
           (float)placed / ship->cargo_count * 100.0f);
}
