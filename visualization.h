/*
 * visualization.h - ASCII cargo layout visualization
 *
 * Provides simple ASCII art output of cargo placement for debugging
 * and presentation purposes.
 */

#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include "cargoforge.h"

/**
 * print_cargo_layout_ascii - Print ASCII visualization of cargo layout
 *
 * Creates a simple top-down view of the ship showing cargo placement.
 * Uses a character grid to represent the ship's deck/holds.
 *
 * @param ship Ship structure with placed cargo
 *
 * Output format:
 * - '#' = cargo
 * - '.' = empty space
 * - '+' = bin boundaries
 */
void print_cargo_layout_ascii(const Ship *ship);

/**
 * print_cargo_summary - Print detailed cargo placement summary
 *
 * Lists all cargo items with their positions and bin assignments.
 *
 * @param ship Ship structure with placed cargo
 */
void print_cargo_summary(const Ship *ship);

#endif /* VISUALIZATION_H */
