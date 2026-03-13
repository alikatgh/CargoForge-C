#ifndef PLACEMENT_2D_H
#define PLACEMENT_2D_H

#include "cargoforge.h"

#define MAX_SHELVES 100

typedef struct {
    float y;
    float height;
    float used_width;
} Shelf;

typedef struct {
    char name[32];
    float x_offset;
    float y_offset;
    float z_offset;
    float w;
    float h;
    float used_height;
    Shelf shelves[MAX_SHELVES];
    int shelf_count;
} Bin;

void place_cargo_2d(Ship *ship);

#endif /* PLACEMENT_2D_H */