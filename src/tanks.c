/*
 * tanks.c - Tank parsing and free surface correction calculations
 */

#include "tanks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int parse_tank_config(const char *filename, TankConfig *config) {
    if (!filename || !config) return -1;

    memset(config, 0, sizeof(*config));

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open tank config '%s'\n", filename);
        return -1;
    }

    char line[512];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Strip \r for Windows line endings */
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
            continue;

        if (config->count >= MAX_TANKS) {
            fprintf(stderr, "Warning: Tank config exceeds %d entries, truncating\n",
                    MAX_TANKS);
            break;
        }

        Tank *t = &config->tanks[config->count];

        /* Parse: id,length,breadth,height,pos_x,pos_y,pos_z,fill,density */
        char id_buf[32];
        int parsed = sscanf(line, "%31[^,],%f,%f,%f,%f,%f,%f,%f,%f",
                            id_buf, &t->length, &t->breadth, &t->height,
                            &t->pos_x, &t->pos_y, &t->pos_z,
                            &t->fill_fraction, &t->density);

        if (parsed < 9) {
            fprintf(stderr, "Warning: Skipping malformed tank entry at line %d\n", line_num);
            continue;
        }

        strncpy(t->id, id_buf, sizeof(t->id) - 1);
        t->id[sizeof(t->id) - 1] = '\0';

        /* Clamp fill fraction */
        if (t->fill_fraction < 0.0f) t->fill_fraction = 0.0f;
        if (t->fill_fraction > 1.0f) t->fill_fraction = 1.0f;

        config->count++;
    }

    fclose(fp);
    return 0;
}

float calculate_free_surface_moment(const Tank *tank) {
    if (!tank) return 0.0f;

    /* No free surface for empty or full tanks */
    if (tank->fill_fraction <= 0.0f || tank->fill_fraction >= 1.0f)
        return 0.0f;

    /* FSM = rho * l * b^3 / 12 for rectangular tank */
    return tank->density * tank->length * tank->breadth * tank->breadth * tank->breadth / 12.0f;
}

float calculate_total_fsm(const TankConfig *config) {
    if (!config) return 0.0f;

    float total = 0.0f;
    for (int i = 0; i < config->count; i++) {
        total += calculate_free_surface_moment(&config->tanks[i]);
    }
    return total;
}

float calculate_virtual_kg_rise(const TankConfig *config, float displacement_t) {
    if (!config || displacement_t < 0.01f) return 0.0f;

    float total_fsm = calculate_total_fsm(config);
    return total_fsm / displacement_t;
}

float calculate_tank_weight(const TankConfig *config) {
    if (!config) return 0.0f;

    float total = 0.0f;
    for (int i = 0; i < config->count; i++) {
        const Tank *t = &config->tanks[i];
        float volume = t->length * t->breadth * t->height * t->fill_fraction;
        total += volume * t->density;
    }
    return total;
}

float calculate_tank_vertical_moment(const TankConfig *config) {
    if (!config) return 0.0f;

    float moment = 0.0f;
    for (int i = 0; i < config->count; i++) {
        const Tank *t = &config->tanks[i];
        float volume = t->length * t->breadth * t->height * t->fill_fraction;
        float weight = volume * t->density;
        /* CG of liquid in a partially filled rectangular tank is at half the fill height */
        float liquid_cg = t->pos_z + (t->height * t->fill_fraction) / 2.0f;
        moment += weight * liquid_cg;
    }
    return moment;
}
