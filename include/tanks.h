/*
 * tanks.h - Tank configuration and free surface correction
 *
 * Partially filled tanks create free surface effects that reduce
 * effective GM. This module calculates the free surface moment (FSM)
 * and virtual rise in KG for rectangular tanks.
 */

#ifndef TANKS_H
#define TANKS_H

#define MAX_TANKS 50

/**
 * Tank - A single liquid tank on the vessel.
 */
typedef struct {
    char  id[32];
    float length;         /* tank length along ship's length (m) */
    float breadth;        /* tank breadth athwartships (m) */
    float height;         /* tank height (m) */
    float pos_x;          /* longitudinal position of tank center from AP (m) */
    float pos_y;          /* transverse position of tank center from CL (m) */
    float pos_z;          /* vertical position of tank bottom above keel (m) */
    float fill_fraction;  /* 0.0 to 1.0 */
    float density;        /* liquid density (t/m3): seawater=1.025, FO=0.95, FW=1.0 */
} Tank;

/**
 * TankConfig - Collection of all tanks on the vessel.
 */
typedef struct TankConfig_ {
    Tank tanks[MAX_TANKS];
    int count;
} TankConfig;

/**
 * parse_tank_config - Parse a CSV tank configuration file.
 *
 * Expected columns:
 *   id, length_m, breadth_m, height_m, pos_x_m, pos_y_m, pos_z_m, fill_fraction, density_t_m3
 *
 * @return 0 on success, -1 on error
 */
int parse_tank_config(const char *filename, TankConfig *config);

/**
 * calculate_free_surface_moment - FSM for a single rectangular tank.
 *
 * For a rectangular tank: FSM = rho * l * b^3 / 12
 * Only applies to partially filled tanks (0 < fill < 1).
 * Full and empty tanks have no free surface effect.
 *
 * @return FSM in t-m
 */
float calculate_free_surface_moment(const Tank *tank);

/**
 * calculate_total_fsm - Sum of FSM for all partially-filled tanks.
 */
float calculate_total_fsm(const TankConfig *config);

/**
 * calculate_virtual_kg_rise - Virtual rise in KG due to free surface.
 *
 * GG' = sum(FSM) / displacement
 *
 * @return virtual KG rise in metres
 */
float calculate_virtual_kg_rise(const TankConfig *config, float displacement_t);

/**
 * calculate_tank_weight - Total weight of liquid in all tanks.
 *
 * @return total tank weight in tonnes
 */
float calculate_tank_weight(const TankConfig *config);

/**
 * calculate_tank_moment - Vertical moment of tank contents about keel.
 *
 * @return moment in t-m
 */
float calculate_tank_vertical_moment(const TankConfig *config);

#endif /* TANKS_H */
