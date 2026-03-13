/*
 * libcargoforge.c - CargoForge embeddable library implementation
 *
 * Wraps the internal modules (parser, placement, analysis, IMDG) behind
 * an opaque-handle API with no global state.  Every CargoForge handle is
 * fully independent — safe to use from multiple threads provided each
 * thread has its own handle.
 */

#include "libcargoforge.h"
#include "cargoforge.h"
#include "placement_3d.h"
#include "imdg.h"
#include "json_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* INTERNAL STATE                                                     */
/* ------------------------------------------------------------------ */

struct CargoForge {
    Ship            ship;
    int             ship_loaded;
    int             cargo_loaded;

    AnalysisResult  analysis;
    int             analyzed;

    IMDGCheckResult imdg;
    int             imdg_checked;

    CfResult        result;       /* public result cache */
    int             result_valid;

    char           *json_cache;   /* cached JSON output */
    char            errmsg[512];
};

/* ------------------------------------------------------------------ */
/* HELPERS                                                            */
/* ------------------------------------------------------------------ */

static void set_error(CargoForge *cf, const char *msg) {
    if (cf && msg)
        snprintf(cf->errmsg, sizeof(cf->errmsg), "%s", msg);
}

static void clear_error(CargoForge *cf) {
    cf->errmsg[0] = '\0';
}

/**
 * Write a string to a temporary file, return the path.
 * Caller must unlink() the path when done.
 * Returns 0 on success, -1 on failure.
 */
static int write_temp_file(const char *text, char *path, size_t pathsz) {
    snprintf(path, pathsz, "/tmp/cargoforge_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) return -1;

    size_t len = strlen(text);
    ssize_t written = write(fd, text, len);
    close(fd);

    if (written < 0 || (size_t)written != len) {
        unlink(path);
        return -1;
    }
    return 0;
}

/** Convert internal AnalysisResult to public CfResult */
static void fill_result(CargoForge *cf) {
    const AnalysisResult *a = &cf->analysis;
    CfResult *r = &cf->result;

    r->placed_count   = a->placed_item_count;
    r->total_count    = cf->ship.cargo_count;
    r->cargo_weight   = a->total_cargo_weight_kg;
    r->displacement   = cf->ship.lightship_weight + a->total_cargo_weight_kg;

    r->draft          = a->draft;
    r->kg             = a->kg;
    r->kb             = a->kb;
    r->bm             = a->bm;
    r->gm             = a->gm;
    r->gm_corrected   = a->gm_corrected;
    r->free_surface_correction = a->free_surface_correction;
    r->hydro_table_used = a->hydro_table_used;

    r->trim           = a->trim;
    r->heel           = a->heel;
    r->lcg            = a->lcg;
    r->cg_longitudinal_pct = a->cg.perc_x;
    r->cg_transverse_pct   = a->cg.perc_y;

    r->gz_at_30       = a->gz_at_30;
    r->gz_max         = a->gz_max;
    r->gz_max_angle   = a->gz_max_angle;
    r->area_0_30      = a->area_0_30;
    r->area_0_40      = a->area_0_40;
    r->area_30_40     = a->area_30_40;
    r->imo_compliant  = a->imo_compliant;

    r->max_shear_force    = a->max_shear_force;
    r->max_bending_moment = a->max_bending_moment;
    r->strength_compliant = a->strength_compliant;

    cf->result_valid = 1;
}

static void invalidate_results(CargoForge *cf) {
    cf->analyzed = 0;
    cf->result_valid = 0;
    cf->imdg_checked = 0;
    if (cf->json_cache) {
        free(cf->json_cache);
        cf->json_cache = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* LIFECYCLE                                                          */
/* ------------------------------------------------------------------ */

int cargoforge_open(CargoForge **out) {
    if (!out) return CF_ERROR;

    CargoForge *cf = calloc(1, sizeof(CargoForge));
    if (!cf) return CF_ERR_NOMEM;

    cf->result.strength_compliant = -1;
    *out = cf;
    return CF_OK;
}

void cargoforge_close(CargoForge *cf) {
    if (!cf) return;
    if (cf->ship_loaded || cf->cargo_loaded)
        ship_cleanup(&cf->ship);
    if (cf->json_cache)
        free(cf->json_cache);
    free(cf);
}

/* ------------------------------------------------------------------ */
/* DATA LOADING                                                       */
/* ------------------------------------------------------------------ */

int cargoforge_load_ship(CargoForge *cf, const char *config_path) {
    if (!cf || !config_path) return CF_ERROR;
    clear_error(cf);

    /* Reset if reloading */
    if (cf->ship_loaded || cf->cargo_loaded) {
        ship_cleanup(&cf->ship);
        memset(&cf->ship, 0, sizeof(cf->ship));
        cf->ship_loaded = 0;
        cf->cargo_loaded = 0;
        invalidate_results(cf);
    }

    if (parse_ship_config(config_path, &cf->ship) != 0) {
        set_error(cf, "Failed to parse ship configuration");
        return CF_ERR_PARSE;
    }

    cf->ship_loaded = 1;
    return CF_OK;
}

int cargoforge_load_cargo(CargoForge *cf, const char *manifest_path) {
    if (!cf || !manifest_path) return CF_ERROR;
    clear_error(cf);

    if (!cf->ship_loaded) {
        set_error(cf, "Ship configuration must be loaded before cargo");
        return CF_ERR_NO_SHIP;
    }

    /* Free existing cargo if reloading */
    if (cf->cargo_loaded) {
        if (cf->ship.cargo) {
            for (int i = 0; i < cf->ship.cargo_count; i++) {
                if (cf->ship.cargo[i].dg) {
                    free(cf->ship.cargo[i].dg);
                    cf->ship.cargo[i].dg = NULL;
                }
            }
            free(cf->ship.cargo);
            cf->ship.cargo = NULL;
        }
        cf->ship.cargo_count = 0;
        cf->ship.cargo_capacity = 0;
        cf->cargo_loaded = 0;
        invalidate_results(cf);
    }

    if (parse_cargo_list(manifest_path, &cf->ship) != 0) {
        set_error(cf, "Failed to parse cargo manifest");
        return CF_ERR_PARSE;
    }

    cf->cargo_loaded = 1;
    return CF_OK;
}

int cargoforge_load_ship_string(CargoForge *cf, const char *config_text) {
    if (!cf || !config_text) return CF_ERROR;

    char tmppath[256];
    if (write_temp_file(config_text, tmppath, sizeof(tmppath)) != 0) {
        set_error(cf, "Failed to create temporary file for ship config");
        return CF_ERR_FILE;
    }

    int rc = cargoforge_load_ship(cf, tmppath);
    unlink(tmppath);
    return rc;
}

int cargoforge_load_cargo_string(CargoForge *cf, const char *manifest_text) {
    if (!cf || !manifest_text) return CF_ERROR;

    char tmppath[256];
    if (write_temp_file(manifest_text, tmppath, sizeof(tmppath)) != 0) {
        set_error(cf, "Failed to create temporary file for cargo manifest");
        return CF_ERR_FILE;
    }

    int rc = cargoforge_load_cargo(cf, tmppath);
    unlink(tmppath);
    return rc;
}

/* ------------------------------------------------------------------ */
/* OPERATIONS                                                         */
/* ------------------------------------------------------------------ */

int cargoforge_optimize(CargoForge *cf) {
    if (!cf) return CF_ERROR;
    clear_error(cf);

    if (!cf->ship_loaded) {
        set_error(cf, "No ship configuration loaded");
        return CF_ERR_NO_SHIP;
    }
    if (!cf->cargo_loaded) {
        set_error(cf, "No cargo manifest loaded");
        return CF_ERR_NO_CARGO;
    }

    invalidate_results(cf);

    /* Run 3D bin-packing */
    place_cargo_3d(&cf->ship);

    /* Run stability analysis */
    cf->analysis = perform_analysis(&cf->ship);
    cf->analyzed = 1;

    fill_result(cf);
    return CF_OK;
}

int cargoforge_analyze(CargoForge *cf) {
    if (!cf) return CF_ERROR;
    clear_error(cf);

    if (!cf->ship_loaded) {
        set_error(cf, "No ship configuration loaded");
        return CF_ERR_NO_SHIP;
    }

    invalidate_results(cf);

    cf->analysis = perform_analysis(&cf->ship);
    cf->analyzed = 1;

    fill_result(cf);
    return CF_OK;
}

int cargoforge_check_imdg(CargoForge *cf) {
    if (!cf) return CF_ERROR;
    clear_error(cf);

    if (!cf->analyzed) {
        set_error(cf, "Must run optimize or analyze before IMDG check");
        return CF_ERR_STATE;
    }

    cf->imdg = imdg_check_all(&cf->ship);
    cf->imdg_checked = 1;
    return CF_OK;
}

void cargoforge_reset(CargoForge *cf) {
    if (!cf) return;

    if (cf->ship_loaded || cf->cargo_loaded)
        ship_cleanup(&cf->ship);

    memset(&cf->ship, 0, sizeof(cf->ship));
    memset(&cf->analysis, 0, sizeof(cf->analysis));
    memset(&cf->imdg, 0, sizeof(cf->imdg));
    memset(&cf->result, 0, sizeof(cf->result));

    cf->ship_loaded = 0;
    cf->cargo_loaded = 0;
    cf->analyzed = 0;
    cf->result_valid = 0;
    cf->imdg_checked = 0;
    cf->result.strength_compliant = -1;

    if (cf->json_cache) {
        free(cf->json_cache);
        cf->json_cache = NULL;
    }

    clear_error(cf);
}

/* ------------------------------------------------------------------ */
/* RESULTS                                                            */
/* ------------------------------------------------------------------ */

const CfResult *cargoforge_result(const CargoForge *cf) {
    if (!cf || !cf->result_valid) return NULL;
    return &cf->result;
}

const char *cargoforge_result_json(CargoForge *cf) {
    if (!cf || !cf->analyzed) return NULL;

    /* Return cached version if available */
    if (cf->json_cache) return cf->json_cache;

    /* Generate JSON into a memory buffer */
    size_t size = 0;
    FILE *stream = open_memstream(&cf->json_cache, &size);
    if (!stream) {
        set_error(cf, "Failed to create memory stream for JSON");
        return NULL;
    }

    fprint_json_output(stream, &cf->ship, &cf->analysis);
    fclose(stream);

    return cf->json_cache;
}

int cargoforge_cargo_info(const CargoForge *cf, int index, CfCargoInfo *info) {
    if (!cf || !info) return CF_ERROR;
    if (index < 0 || index >= cf->ship.cargo_count) return CF_ERROR;

    const Cargo *c = &cf->ship.cargo[index];

    info->id     = c->id;
    info->weight = c->weight;
    info->length = c->dimensions[0];
    info->width  = c->dimensions[1];
    info->height = c->dimensions[2];
    info->type   = c->type;
    info->placed = (c->pos_x >= 0) ? 1 : 0;
    info->pos_x  = c->pos_x;
    info->pos_y  = c->pos_y;
    info->pos_z  = c->pos_z;

    if (c->dg) {
        info->dg_class    = c->dg->dg_class;
        info->dg_division = c->dg->dg_division;
        info->un_number   = c->dg->un_number;
    } else {
        info->dg_class    = 0;
        info->dg_division = 0;
        info->un_number   = NULL;
    }

    return CF_OK;
}

int cargoforge_cargo_count(const CargoForge *cf) {
    if (!cf) return 0;
    return cf->ship.cargo_count;
}

int cargoforge_imdg_violation_count(const CargoForge *cf) {
    if (!cf || !cf->imdg_checked) return -1;
    return cf->imdg.violation_count;
}

int cargoforge_imdg_violation(const CargoForge *cf, int index,
                              CfIMDGViolation *v) {
    if (!cf || !v || !cf->imdg_checked) return CF_ERROR;
    if (index < 0 || index >= cf->imdg.violation_count) return CF_ERROR;

    const IMDGViolation *iv = &cf->imdg.violations[index];
    v->cargo_a             = iv->cargo_idx_a;
    v->cargo_b             = iv->cargo_idx_b;
    v->required_segregation = (int)iv->required;
    v->actual_distance     = iv->actual_distance;
    v->description         = iv->description;

    return CF_OK;
}

int cargoforge_imdg_compliant(const CargoForge *cf) {
    if (!cf || !cf->imdg_checked) return -1;
    return cf->imdg.compliant;
}

/* ------------------------------------------------------------------ */
/* ERROR HANDLING                                                     */
/* ------------------------------------------------------------------ */

const char *cargoforge_errmsg(const CargoForge *cf) {
    if (!cf) return "NULL context";
    return cf->errmsg;
}

const char *cargoforge_errstr(int code) {
    switch (code) {
        case CF_OK:            return "Success";
        case CF_ERROR:         return "Generic error";
        case CF_ERR_NOMEM:     return "Memory allocation failed";
        case CF_ERR_FILE:      return "File error";
        case CF_ERR_PARSE:     return "Parse error";
        case CF_ERR_NO_SHIP:   return "No ship configuration loaded";
        case CF_ERR_NO_CARGO:  return "No cargo manifest loaded";
        case CF_ERR_OVERWEIGHT: return "Total weight exceeds ship capacity";
        case CF_ERR_STATE:     return "Invalid operation for current state";
        default:               return "Unknown error";
    }
}

/* ------------------------------------------------------------------ */
/* UTILITY                                                            */
/* ------------------------------------------------------------------ */

const char *cargoforge_version(void) {
    return CF_VERSION_STRING;
}
