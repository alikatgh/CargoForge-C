/*
 * json_output.c - JSON output implementation
 *
 * Simple JSON generation without external dependencies.
 * Outputs valid JSON for web API consumption.
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

void print_json_output(const Ship *ship, const AnalysisResult *result) {
    char escaped[128];

    printf("{\n");

    // Ship specifications
    printf("  \"ship\": {\n");
    printf("    \"length\": %.2f,\n", ship->length);
    printf("    \"width\": %.2f,\n", ship->width);
    printf("    \"max_weight\": %.2f,\n", ship->max_weight);
    printf("    \"lightship_weight\": %.2f,\n", ship->lightship_weight);
    printf("    \"lightship_kg\": %.2f\n", ship->lightship_kg);
    printf("  },\n");

    // Cargo placements
    printf("  \"cargo\": [\n");
    for (int i = 0; i < ship->cargo_count; i++) {
        const Cargo *c = &ship->cargo[i];

        escape_json_string(c->id, escaped, sizeof(escaped));
        printf("    {\n");
        printf("      \"id\": \"%s\",\n", escaped);
        printf("      \"weight\": %.2f,\n", c->weight);
        printf("      \"dimensions\": [%.2f, %.2f, %.2f],\n",
               c->dimensions[0], c->dimensions[1], c->dimensions[2]);

        escape_json_string(c->type, escaped, sizeof(escaped));
        printf("      \"type\": \"%s\",\n", escaped);

        if (c->pos_x >= 0) {
            printf("      \"position\": {\"x\": %.2f, \"y\": %.2f, \"z\": %.2f},\n",
                   c->pos_x, c->pos_y, c->pos_z);
            printf("      \"placed\": true\n");
        } else {
            printf("      \"position\": null,\n");
            printf("      \"placed\": false\n");
        }

        printf("    }%s\n", (i < ship->cargo_count - 1) ? "," : "");
    }
    printf("  ],\n");

    // Analysis results
    printf("  \"analysis\": {\n");
    printf("    \"placed_count\": %d,\n", result->placed_item_count);
    printf("    \"total_count\": %d,\n", ship->cargo_count);
    printf("    \"total_cargo_weight\": %.2f,\n", result->total_cargo_weight_kg);
    printf("    \"total_ship_weight\": %.2f,\n",
           ship->lightship_weight + result->total_cargo_weight_kg);
    printf("    \"capacity_used_percent\": %.2f,\n",
           ((ship->lightship_weight + result->total_cargo_weight_kg) / ship->max_weight) * 100.0f);

    printf("    \"center_of_gravity\": {\n");
    printf("      \"longitudinal_percent\": %.2f,\n", result->cg.perc_x);
    printf("      \"transverse_percent\": %.2f\n", result->cg.perc_y);
    printf("    },\n");

    if (!isnan(result->gm)) {
        printf("    \"metacentric_height\": %.2f,\n", result->gm);

        // Stability classification
        const char *stability;
        if (result->gm < 0.3f) {
            stability = "critical";
        } else if (result->gm > 3.0f) {
            stability = "overstiff";
        } else if (result->gm >= 0.5f && result->gm <= 2.5f) {
            stability = "optimal";
        } else {
            stability = "acceptable";
        }
        printf("    \"stability_status\": \"%s\",\n", stability);

        // Balance status
        const char *balance;
        if (result->cg.perc_x >= 45 && result->cg.perc_x <= 55 &&
            result->cg.perc_y >= 40 && result->cg.perc_y <= 60) {
            balance = "good";
        } else {
            balance = "warning";
        }
        printf("    \"balance_status\": \"%s\",\n", balance);

        printf("    \"overweight\": false\n");
    } else {
        printf("    \"metacentric_height\": null,\n");
        printf("    \"stability_status\": \"rejected\",\n");
        printf("    \"balance_status\": \"unknown\",\n");
        printf("    \"overweight\": true\n");
    }

    printf("  }\n");
    printf("}\n");
}
