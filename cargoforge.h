/* cargoforge.h - FINAL VERSION */
#ifndef CARGOFORGE_H
#define CARGOFORGE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CONSTANTS */
#define CARGOFORGE_VERSION "1.0.0"
#define MAX_LINE_LENGTH 256
#define MAX_DIMENSION 3
#define MAX_SHELVES 100
#define DEFAULT_HOLDS 2   /* below-deck holds when the config omits `holds=` */
#define MAX_HOLDS 50      /* upper bound on configurable holds */

/* DATA STRUCTURES */
typedef struct { float x; float y; } Point;
typedef struct { float y; float height; float used_width; } Shelf;

typedef struct {
    char name[32];
    float x_offset, y_offset, z_offset, w, h, used_height;
    Shelf shelves[MAX_SHELVES];
    int shelf_count;
} Bin;

typedef struct {
    char  id[32];
    float weight;
    float dimensions[MAX_DIMENSION];
    char  type[16];
    float pos_x, pos_y, pos_z;
    float placed_w; // Actual width after placement (accounts for rotation)
    float placed_h; // Actual height/depth after placement
    // Optional attributes from the manifest's comma-separated 5th field.
    bool  fragile;      // must stay on top; nothing stacked on it
    bool  stackable;    // may bear stacked cargo
    bool  priority;     // must-load: a prominent warning if it cannot be placed
    bool  reefer;       // refrigerated: draws reefer power
    bool  out_of_gauge; // exceeds standard envelope
    int   dg_class;     // dangerous-goods class 1-9, 0 = none
} Cargo;

typedef struct {
    float length, width, max_weight;
    int   cargo_count, cargo_capacity;
    Cargo *cargo;
    float lightship_weight, lightship_kg;
    int   hold_count;   /* below-deck holds; 0 means "use DEFAULT_HOLDS" */
    float depth;        /* moulded hull depth (m), optional; enables freeboard. 0 = unset */
} Ship;

typedef struct { float perc_x, perc_y; } CG;

typedef struct {
    CG    cg;                      /* combined CG as % of length / beam */
    float gm;                      /* transverse metacentric height (m) */
    float gml;                     /* longitudinal metacentric height (m) */
    float kg, kb, bm;              /* vertical CG, centre of buoyancy, metacentric radius (m) */
    float draft_mean;              /* mean draft (m) */
    float draft_fore, draft_aft;   /* draft at the ends after trim (m) */
    float trim;                    /* draft_aft - draft_fore (m); + = trim by stern */
    float heel_deg;                /* static list angle (deg); + = to starboard */
    float displacement_t;          /* total weight afloat (t) */
    float deadweight_t;            /* cargo deadweight (t) */
    float volume_m3;               /* underwater (displaced) volume (m^3) */
    float freeboard;               /* depth - mean draft (m); NAN if depth unset */
    float fs_correction;           /* free-surface reduction of GM (m) */
    float gm_fluid;                /* GM corrected for free surface (m) */
    float gz30;                    /* righting arm GZ at 30 deg (m) */
    float gm_margin;               /* gm_fluid - required minimum GM (m) */
    float wind_heel_deg;           /* steady beam-wind heel (deg); NAN if depth unset */
    float deck_edge_deg;           /* deck-edge immersion angle (deg); NAN if depth unset */
    float total_cargo_weight_kg;
    int   placed_item_count;
} AnalysisResult;

#define MIN_GM 0.15f               /* minimum acceptable metacentric height (m) */

/* CLI output preferences for the human-readable report. */
typedef struct {
    bool color;      /* wrap verdicts in ANSI color */
    int  verbosity;  /* -1 = quiet (summary only), 0 = normal, 1 = verbose */
    bool diagram;    /* render the ASCII stowage plan + utilization + gauge */
} OutputOptions;


/* FUNCTION PROTOTYPES */
int parse_ship_config(const char *filename, Ship *ship);
int parse_cargo_list(const char *filename, Ship *ship);
void optimize_cargo_placement(Ship *ship);
AnalysisResult perform_analysis(const Ship *ship);
void print_loading_plan(const Ship *ship, const OutputOptions *opt);
void print_loading_plan_json(const Ship *ship);
void print_loading_plan_csv(const Ship *ship);
void print_loading_plan_md(const Ship *ship);
void usage(const char *prog_name);
void place_cargo_2d(Ship *ship);
Point calculate_cg_for_placement(const Ship *ship, const Cargo *new_item, Point new_pos, float item_w, float item_h);

#endif /* CARGOFORGE_H */
