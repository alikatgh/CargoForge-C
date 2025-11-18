#ifndef PLACEMENT_2D_H
#define PLACEMENT_2D_H

#include "cargoforge.h"

/**
 * @brief Places cargo items using a 2D FFD heuristic across multiple bins.
 *
 * This function sorts a ship's cargo by weight (descending) and attempts to
 * place each item into hardcoded holds or the deck. It tries both original
 * and rotated orientations for a better fit. The function modifies the
 * ship's cargo array in-place, updating the pos_x, pos_y, and pos_z fields.
 *
 * @param ship A pointer to the Ship struct, which contains the cargo to be placed.
 */
void place_cargo_2d(Ship *ship);

#endif // PLACEMENT_2D_H