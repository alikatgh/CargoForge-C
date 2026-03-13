/*
 * json_output.c - JSON output implementation
 */

#include "json_output.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void escape_json_string(const char *str, char *buffer, size_t buffer_size) {
    size_t i = 0, j = 0;

    while (str[i] != '\0' && j < buffer_size - 2) {
        if (str[i] == '"' || str[i] == '\\') {
            buffer[j++] = '\\';
        }
        buffer[j++] = str[i++];
    }
    buffer[j] = '\0';
}

void fprint_json_output(FILE *fp, const Ship *ship, const AnalysisResult *result) {
    char escaped[128];

    fprintf(fp,"{\n");

    /* Ship specifications */
    fprintf(fp,"  \"ship\": {\n");
    fprintf(fp,"    \"length\": %.2f,\n", ship->length);
    fprintf(fp,"    \"width\": %.2f,\n", ship->width);
    fprintf(fp,"    \"max_weight\": %.2f,\n", ship->max_weight);
    fprintf(fp,"    \"lightship_weight\": %.2f,\n", ship->lightship_weight);
    fprintf(fp,"    \"lightship_kg\": %.2f\n", ship->lightship_kg);
    fprintf(fp,"  },\n");

    /* Cargo placements */
    fprintf(fp,"  \"cargo\": [\n");
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];

        escape_json_string(c->id, escaped, sizeof(escaped));
        fprintf(fp,"    {\n");
        fprintf(fp,"      \"id\": \"%s\",\n", escaped);
        fprintf(fp,"      \"weight\": %.2f,\n", c->weight);
        fprintf(fp,"      \"dimensions\": [%.2f, %.2f, %.2f],\n",
               c->dimensions[0], c->dimensions[1], c->dimensions[2]);

        escape_json_string(c->type, escaped, sizeof(escaped));
        fprintf(fp,"      \"type\": \"%s\",\n", escaped);

        if (c->pos_x >= 0) {
            fprintf(fp,"      \"position\": {\"x\": %.2f, \"y\": %.2f, \"z\": %.2f},\n",
                   c->pos_x, c->pos_y, c->pos_z);
            fprintf(fp,"      \"placed\": true\n");
        } else {
            fprintf(fp,"      \"position\": null,\n");
            fprintf(fp,"      \"placed\": false\n");
        }

        fprintf(fp,"    }%s\n", (i < ship->cargo_count - 1) ? "," : "");
    }
    fprintf(fp,"  ],\n");

    /* Analysis results */
    fprintf(fp,"  \"analysis\": {\n");
    fprintf(fp,"    \"placed_count\": %d,\n", result->placed_item_count);
    fprintf(fp,"    \"total_count\": %d,\n", ship->cargo_count);
    fprintf(fp,"    \"total_cargo_weight\": %.2f,\n", result->total_cargo_weight_kg);
    fprintf(fp,"    \"total_ship_weight\": %.2f,\n",
           ship->lightship_weight + result->total_cargo_weight_kg);
    fprintf(fp,"    \"capacity_used_percent\": %.2f,\n",
           ((ship->lightship_weight + result->total_cargo_weight_kg) / ship->max_weight) * 100.0f);

    fprintf(fp,"    \"center_of_gravity\": {\n");
    fprintf(fp,"      \"longitudinal_percent\": %.2f,\n", result->cg.perc_x);
    fprintf(fp,"      \"transverse_percent\": %.2f\n", result->cg.perc_y);
    fprintf(fp,"    },\n");

    if (!isnan(result->gm)) {
        /* Hydrostatics */
        fprintf(fp,"    \"hydrostatics\": {\n");
        fprintf(fp,"      \"draft\": %.3f,\n", result->draft);
        fprintf(fp,"      \"kg\": %.3f,\n", result->kg);
        fprintf(fp,"      \"kb\": %.3f,\n", result->kb);
        fprintf(fp,"      \"bm\": %.3f,\n", result->bm);
        fprintf(fp,"      \"gm\": %.3f,\n", result->gm);
        fprintf(fp,"      \"free_surface_correction\": %.3f,\n", result->free_surface_correction);
        fprintf(fp,"      \"gm_corrected\": %.3f,\n", result->gm_corrected);
        fprintf(fp,"      \"hydro_table_used\": %s\n", result->hydro_table_used ? "true" : "false");
        fprintf(fp,"    },\n");

        /* Trim and heel */
        fprintf(fp,"    \"trim\": %.4f,\n", result->trim);
        fprintf(fp,"    \"heel\": %.3f,\n", result->heel);
        fprintf(fp,"    \"lcg_from_midship\": %.3f,\n", result->lcg);

        /* IMO criteria */
        fprintf(fp,"    \"imo_stability\": {\n");
        fprintf(fp,"      \"gz_at_30\": %.4f,\n", result->gz_at_30);
        fprintf(fp,"      \"gz_max\": %.4f,\n", result->gz_max);
        fprintf(fp,"      \"gz_max_angle\": %.1f,\n", result->gz_max_angle);
        fprintf(fp,"      \"area_0_30\": %.5f,\n", result->area_0_30);
        fprintf(fp,"      \"area_0_40\": %.5f,\n", result->area_0_40);
        fprintf(fp,"      \"area_30_40\": %.5f,\n", result->area_30_40);
        fprintf(fp,"      \"compliant\": %s\n", result->imo_compliant ? "true" : "false");
        fprintf(fp,"    },\n");

        /* Stability classification */
        const char *stability;
        if (result->gm < 0.3f) stability = "critical";
        else if (result->gm > 3.0f) stability = "overstiff";
        else if (result->gm >= 0.5f && result->gm <= 2.5f) stability = "optimal";
        else stability = "acceptable";
        fprintf(fp,"    \"stability_status\": \"%s\",\n", stability);

        const char *balance;
        if (result->cg.perc_x >= 45 && result->cg.perc_x <= 55 &&
            result->cg.perc_y >= 40 && result->cg.perc_y <= 60)
            balance = "good";
        else
            balance = "warning";
        fprintf(fp,"    \"balance_status\": \"%s\",\n", balance);

        /* Longitudinal strength */
        if (result->strength_compliant >= 0) {
            fprintf(fp,"    \"longitudinal_strength\": {\n");
            fprintf(fp,"      \"max_shear_force\": %.1f,\n", result->max_shear_force);
            fprintf(fp,"      \"max_bending_moment\": %.1f,\n", result->max_bending_moment);
            fprintf(fp,"      \"compliant\": %s\n", result->strength_compliant ? "true" : "false");
            fprintf(fp,"    },\n");
        }

        fprintf(fp,"    \"overweight\": false\n");
    } else {
        fprintf(fp,"    \"hydrostatics\": null,\n");
        fprintf(fp,"    \"trim\": null,\n");
        fprintf(fp,"    \"heel\": null,\n");
        fprintf(fp,"    \"imo_stability\": null,\n");
        fprintf(fp,"    \"stability_status\": \"rejected\",\n");
        fprintf(fp,"    \"balance_status\": \"unknown\",\n");
        fprintf(fp,"    \"overweight\": true\n");
    }

    fprintf(fp,"  }\n");
    fprintf(fp,"}\n");
}
