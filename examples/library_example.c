/*
 * library_example.c - Example of using libcargoforge as an embedded library
 *
 * Build:
 *   make lib
 *   gcc -Iinclude -o library_example examples/library_example.c -L. -lcargoforge -lm
 *
 * Run:
 *   ./library_example
 */

#include "libcargoforge.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("CargoForge Library Example (v%s)\n\n", cargoforge_version());

    /* Create context */
    CargoForge *cf;
    if (cargoforge_open(&cf) != CF_OK) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    /* Load ship configuration from a string */
    const char *ship =
        "length_m=180\n"
        "width_m=32\n"
        "max_weight_tonnes=50000\n"
        "lightship_weight_tonnes=12000\n"
        "lightship_kg_m=7.5\n";

    if (cargoforge_load_ship_string(cf, ship) != CF_OK) {
        fprintf(stderr, "Ship load error: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    /* Load cargo manifest from a string */
    const char *cargo =
        "BOX1 25000 12.0x2.4x2.6 standard\n"
        "BOX2 18000 6.0x2.4x2.6 standard\n"
        "BOX3 22000 12.0x2.4x2.6 standard\n"
        "FUEL 5000 3.0x2.0x1.5 hazardous DG:3.0:UN1203:any:F-E\n"
        "FOOD 15000 12.0x2.4x2.9 reefer\n";

    if (cargoforge_load_cargo_string(cf, cargo) != CF_OK) {
        fprintf(stderr, "Cargo load error: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    /* Run optimization */
    if (cargoforge_optimize(cf) != CF_OK) {
        fprintf(stderr, "Optimization error: %s\n", cargoforge_errmsg(cf));
        cargoforge_close(cf);
        return 1;
    }

    /* Read results */
    const CfResult *r = cargoforge_result(cf);
    printf("Placed: %d / %d\n", r->placed_count, r->total_count);
    printf("Draft:  %.2f m\n", r->draft);
    printf("GM:     %.2f m (corrected: %.2f m)\n", r->gm, r->gm_corrected);
    printf("Trim:   %.3f m\n", r->trim);
    printf("Heel:   %.2f deg\n", r->heel);
    printf("IMO:    %s\n", r->imo_compliant ? "COMPLIANT" : "NON-COMPLIANT");

    /* Print individual cargo positions */
    printf("\nCargo placements:\n");
    for (int i = 0; i < cargoforge_cargo_count(cf); i++) {
        CfCargoInfo info;
        cargoforge_cargo_info(cf, i, &info);
        if (info.placed) {
            printf("  %-12s -> (%.1f, %.1f, %.1f)\n",
                   info.id, info.pos_x, info.pos_y, info.pos_z);
        } else {
            printf("  %-12s -> NOT PLACED\n", info.id);
        }
    }

    /* IMDG check */
    cargoforge_check_imdg(cf);
    int violations = cargoforge_imdg_violation_count(cf);
    printf("\nIMDG: %d violation(s) — %s\n",
           violations, cargoforge_imdg_compliant(cf) ? "COMPLIANT" : "VIOLATIONS");

    /* Get full JSON */
    const char *json = cargoforge_result_json(cf);
    printf("\nJSON output length: %zu bytes\n", json ? strlen(json) : 0);

    cargoforge_close(cf);
    return 0;
}
