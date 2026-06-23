/*
 * parser.c - File parsing logic for CargoForge-C.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h> // For NAN
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cargoforge.h"

/**
 * @brief Opens a path for reading, or stdin when the path is "-".
 *
 * The cargo parser makes two passes (rewind), but stdin is not seekable, so for
 * "-" we copy stdin into an anonymous temp file (auto-deleted on close) and return
 * that — giving a seekable stream for both real files and stdin. Only one input
 * may be "-" per run (there is a single stdin).
 *
 * @return an open, seekable FILE* (fclose when done), or NULL on failure.
 */
static FILE *open_input(const char *path) {
    if (strcmp(path, "-") != 0) return fopen(path, "r");
    FILE *tmp = tmpfile();
    if (!tmp) return NULL;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, stdin)) > 0) {
        if (fwrite(buf, 1, n, tmp) != n) { fclose(tmp); return NULL; }
    }
    rewind(tmp);
    return tmp;
}

/**
 * @brief Safely parses a string into a positive float within a given range.
 *
 * Uses strtof for robust error checking against malformed strings and overflows.
 * Prints an error and returns NAN on failure.
 *
 * @param s The string to parse.
 * @param min The minimum acceptable value.
 * @param max The maximum acceptable value.
 * @param field_name The name of the field being parsed, for error messages.
 * @return The parsed float value, or NAN on error.
 */

 static float safe_atof(const char *s, float min, float max, const char *field_name) {
    char *end = NULL;
    errno = 0; // Reset errno before the call
    float val = strtof(s, &end);

    // Check for conversion errors or out-of-range values
    if (errno != 0 || end == s || (*end != '\0' && *end != '\n') || val < min || val > max) {
        fprintf(stderr, "Error: Invalid or out-of-range %s value '%s'\n", field_name, s);
        return NAN;
    }
    return val;
}

/**
 * @brief Reads one logical line into buf, draining any overflow.
 *
 * If a line is longer than the buffer, fgets would leave the remainder in the
 * stream to be returned as a phantom second line and misparsed. This helper
 * detects that case, discards the rest of the over-long line, and sets
 * *truncated so the caller can skip the record cleanly. First and second parse
 * passes therefore see an identical sequence of logical lines.
 *
 * @return buf on success, or NULL at end of file.
 */
static char *read_line(FILE *file, char *buf, size_t size, bool *truncated) {
    *truncated = false;
    if (!fgets(buf, (int)size, file)) return NULL;
    if (!strchr(buf, '\n') && !feof(file)) {
        *truncated = true; // line didn't fit; drain it to the newline (or EOF)
        int ch;
        while ((ch = fgetc(file)) != '\n' && ch != EOF) { /* discard */ }
    }
    return buf;
}

// Trims leading and trailing whitespace in place, returning the trimmed start.
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
    return s;
}

// Case-insensitive test that `path` ends with `ext` (e.g. ".csv").
static bool has_ext(const char *path, const char *ext) {
    size_t lp = strlen(path), le = strlen(ext);
    if (lp < le) return false;
    const char *tail = path + (lp - le);
    for (size_t i = 0; i < le; i++)
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)ext[i])) return false;
    return true;
}

// Replaces every comma with a space, so a CSV row reuses the whitespace parser.
static void csv_to_spaces(char *s) {
    for (; *s; ++s) if (*s == ',') *s = ' ';
}

// True if a (space-separated) line is a CSV data row: its 2nd token is numeric.
// Filters out a header row and blanks/comments without aborting the parse.
static bool csv_is_data(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\0' || *p == '\n') return false;
    while (*p && *p != ' ' && *p != '\t') p++; // skip the id token
    while (*p == ' ' || *p == '\t') p++;
    return (*p == '-' || *p == '+' || *p == '.' || (*p >= '0' && *p <= '9'));
}

// Finds `"key": <number>` in a flat JSON buffer. Returns 1 and sets *out on success.
static int json_number(const char *buf, const char *key, float *out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(buf, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return 0;
    p++;
    char *end;
    errno = 0;
    float v = strtof(p, &end);
    if (end == p || errno != 0) return 0;
    *out = v;
    return 1;
}

// Parses a flat JSON ship config: {"length_m": 150, "width_m": 25, ...}.
static int parse_ship_config_json(const char *filename, Ship *ship) {
    FILE *file = fopen(filename, "r");
    if (!file) { perror("Error opening ship config file"); return -1; }
    fseek(file, 0, SEEK_END);
    long sz = ftell(file);
    if (sz < 0 || sz > (1L << 20)) { fclose(file); fprintf(stderr, "Error: JSON config unreadable or too large.\n"); return -1; }
    rewind(file);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(file); return -1; }
    size_t got = fread(buf, 1, (size_t)sz, file);
    buf[got] = '\0';
    fclose(file);

    float v;
    if (json_number(buf, "length_m", &v)) ship->length = v;
    if (json_number(buf, "width_m", &v)) ship->width = v;
    if (json_number(buf, "max_weight_tonnes", &v)) ship->max_weight = v * 1000.0f;
    if (json_number(buf, "lightship_weight_tonnes", &v)) ship->lightship_weight = v * 1000.0f;
    if (json_number(buf, "lightship_kg_m", &v)) ship->lightship_kg = v;
    if (json_number(buf, "depth_m", &v)) ship->depth = v;
    if (json_number(buf, "hold_depth_m", &v)) ship->hold_depth = v;
    if (json_number(buf, "max_hold_weight_tonnes", &v)) ship->max_hold_weight = v * 1000.0f;
    if (json_number(buf, "holds", &v)) {
        int h = (int)v;
        if (h < 1 || h > MAX_HOLDS) { fprintf(stderr, "Error: holds must be 1..%d.\n", MAX_HOLDS); free(buf); return -1; }
        ship->hold_count = h;
    }
    free(buf);

    if (ship->length < 0.1f || ship->width < 0.1f || ship->max_weight <= 0.0f) {
        fprintf(stderr, "Error: JSON ship config '%s' is missing length_m, width_m, or max_weight_tonnes.\n", filename);
        return -1;
    }
    return 0;
}

// Parses ship configuration using the safe parser.
int parse_ship_config(const char *filename, Ship *ship) {
    if (has_ext(filename, ".json")) return parse_ship_config_json(filename, ship);

    FILE *file = open_input(filename);
    if (!file) {
        perror("Error opening ship config file");
        return -1;
    }

    /* Track which required keys were seen so we can reject an incomplete config.
     * A missing width_m (etc.) would otherwise leave the field at 0 and divide
     * by zero in the downstream draft / GM math. */
    bool seen_length = false, seen_width = false, seen_max_weight = false;

    char line[MAX_LINE_LENGTH];
    bool truncated;
    while (read_line(file, line, sizeof(line), &truncated)) {
        if (truncated) {
            fprintf(stderr, "Warning: Skipping over-long config line (>%d chars).\n", MAX_LINE_LENGTH - 1);
            continue;
        }
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;

        // Allow `key = value   # inline comment` and surrounding whitespace.
        char *hash = strchr(value, '#');
        if (hash) *hash = '\0';
        key = trim(key);
        value = trim(value);
        if (*value == '\0') continue; // nothing but a comment after '='

        float v = safe_atof(value, 0.1f, 1e9f, key);
        if (isnan(v)) {
            fclose(file);
            return -1; // Abort on invalid data
        }

        if (strcmp(key, "length_m") == 0)                  { ship->length = v;              seen_length = true; }
        else if (strcmp(key, "width_m") == 0)              { ship->width = v;               seen_width = true; }
        else if (strcmp(key, "max_weight_tonnes") == 0)    { ship->max_weight = v * 1000.0f; seen_max_weight = true; }
        else if (strcmp(key, "lightship_weight_tonnes") == 0) ship->lightship_weight = v * 1000.0f;
        else if (strcmp(key, "lightship_kg_m") == 0)       ship->lightship_kg = v;
        else if (strcmp(key, "depth_m") == 0)              ship->depth = v;
        else if (strcmp(key, "hold_depth_m") == 0)         ship->hold_depth = v;
        else if (strcmp(key, "max_hold_weight_tonnes") == 0) ship->max_hold_weight = v * 1000.0f;
        else if (strcmp(key, "holds") == 0) {
            int h = (int)v;
            if (h < 1 || h > MAX_HOLDS) {
                fprintf(stderr, "Error: holds must be between 1 and %d (got '%s').\n", MAX_HOLDS, value);
                fclose(file);
                return -1;
            }
            ship->hold_count = h;
        }
        else fprintf(stderr, "Warning: Unknown ship config key '%s' (ignored).\n", key);
    }
    fclose(file);

    if (!seen_length || !seen_width || !seen_max_weight) {
        fprintf(stderr,
                "Error: Ship config '%s' is missing required field(s):%s%s%s\n",
                filename,
                seen_length ? "" : " length_m",
                seen_width ? "" : " width_m",
                seen_max_weight ? "" : " max_weight_tonnes");
        return -1;
    }
    return 0;
}

// Parses a cargo list using the safe parser.
int parse_cargo_list(const char *filename, Ship *ship) {
    FILE *file = open_input(filename);
    if (!file) {
        perror("Error opening cargo list file");
        return -1;
    }

    // A .csv manifest reuses the whitespace parser: commas become spaces and the
    // header row (and any non-data line) is skipped. Dimensions stay one LxWxH cell.
    bool csv = has_ext(filename, ".csv");

    // First pass: count lines to determine memory allocation size. Over-long lines
    // are skipped here too, so the count matches what the second pass will accept.
    int count = 0;
    char line[MAX_LINE_LENGTH];
    bool truncated;
    while (read_line(file, line, sizeof(line), &truncated)) {
        if (truncated) continue;
        if (csv) {
            csv_to_spaces(line);
            if (csv_is_data(line)) count++;
        } else if (line[0] != '#' && line[0] != '\n') {
            count++;
        }
    }

    if (count == 0) {
        fprintf(stderr, "Error: Cargo list '%s' contains no cargo entries.\n", filename);
        fclose(file);
        return -1;
    }

    ship->cargo = malloc(count * sizeof(Cargo));
    if (!ship->cargo) {
        fprintf(stderr, "Error: Failed to allocate memory for cargo.\n");
        fclose(file);
        return -1;
    }
    ship->cargo_capacity = count;
    ship->cargo_count = 0;

    // Second pass: parse data into structs
    rewind(file);
    int line_num = 0;
    while (read_line(file, line, sizeof(line), &truncated) && ship->cargo_count < ship->cargo_capacity) {
        line_num++;
        if (truncated) {
            fprintf(stderr, "Warning: Skipping over-long cargo line %d (>%d chars).\n", line_num, MAX_LINE_LENGTH - 1);
            continue;
        }
        if (csv) {
            csv_to_spaces(line);
            if (!csv_is_data(line)) continue; // header / blank / comment
        } else if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        char *saveptr;
        char *id = strtok_r(line, " \t", &saveptr);
        char *w_str = strtok_r(NULL, " \t", &saveptr);
        char *dim_str = strtok_r(NULL, " \t", &saveptr);
        char *type = strtok_r(NULL, " \t\n", &saveptr);
        char *attrs = strtok_r(NULL, " \t\n", &saveptr); // optional 5th field

        if (!id || !w_str || !dim_str || !type) {
            fprintf(stderr, "Warning: Skipping malformed cargo data on line %d.\n", line_num);
            continue;
        }

        Cargo *c = &ship->cargo[ship->cargo_count];

        /* malloc does not zero memory. The placement and analysis stages use
         * pos_x < 0 as the "not yet placed" sentinel, so it MUST be set before
         * any item is read back. Leaving these uninitialized is undefined
         * behavior that silently corrupts the CG optimizer. */
        c->pos_x = c->pos_y = c->pos_z = -1.0f;
        c->placed_w = c->placed_h = 0.0f;
        c->fragile = c->priority = c->reefer = c->out_of_gauge = false;
        c->stackable = true; // most cargo bears stacking unless flagged otherwise
        c->dg_class = 0;
        c->temp_c = c->max_stack_t = NAN;
        c->dest[0] = '\0';

        strncpy(c->id, id, sizeof(c->id) - 1);
        c->id[sizeof(c->id) - 1] = '\0';
        strncpy(c->type, type, sizeof(c->type) - 1);
        c->type[sizeof(c->type) - 1] = '\0';

        float weight_t = safe_atof(w_str, 0.1f, 1e6f, "weight");
        if (isnan(weight_t)) {
            free(ship->cargo);
            ship->cargo = NULL; // avoid a dangling pointer; the caller also frees
            fclose(file);
            return -1;
        }
        c->weight = weight_t * 1000.0f; // tonnes -> kg

        // Parse dimensions (e.g., "12.2x2.4x2.6")
        char *dim_saveptr;
        char *tok = strtok_r(dim_str, "x", &dim_saveptr);
        bool dims_ok = true;
        for (int i = 0; i < MAX_DIMENSION; ++i) {
            if (!tok) { dims_ok = false; break; }
            float d = safe_atof(tok, 0.1f, 1e4f, "dimension");
            if (isnan(d)) { dims_ok = false; break; }
            c->dimensions[i] = d;
            tok = strtok_r(NULL, "x", &dim_saveptr);
        }

        if (!dims_ok) {
            fprintf(stderr, "Error: Incomplete or invalid dimensions for cargo '%s' on line %d\n", id, line_num);
            free(ship->cargo);
            ship->cargo = NULL; // avoid a dangling pointer; the caller also frees
            fclose(file);
            return -1;
        }

        // Sanity-check density: above ~15 t/m³ (denser than lead) likely a data error.
        float vol = c->dimensions[0] * c->dimensions[1] * c->dimensions[2];
        if (vol > 0.0f && c->weight / vol > 15000.0f) {
            fprintf(stderr, "Warning: Cargo '%s' has implausibly high density (%.0f kg/m³).\n",
                    id, c->weight / vol);
        }

        // Optional comma-separated attributes, e.g. "priority,reefer,dg=3,fragile".
        if (attrs) {
            char *asave;
            for (char *a = strtok_r(attrs, ",", &asave); a; a = strtok_r(NULL, ",", &asave)) {
                if (strcmp(a, "fragile") == 0)        { c->fragile = true; c->stackable = false; }
                else if (strcmp(a, "nostack") == 0)   c->stackable = false;
                else if (strcmp(a, "stackable") == 0) c->stackable = true;
                else if (strcmp(a, "priority") == 0)  c->priority = true;
                else if (strcmp(a, "reefer") == 0)    c->reefer = true;
                else if (strcmp(a, "oog") == 0)       c->out_of_gauge = true;
                else if (strncmp(a, "dg=", 3) == 0) {
                    int d = atoi(a + 3);
                    if (d >= 1 && d <= 9) c->dg_class = d;
                    else fprintf(stderr, "Warning: invalid DG class '%s' for cargo '%s'.\n", a, id);
                }
                else if (strncmp(a, "temp=", 5) == 0)     { c->temp_c = strtof(a + 5, NULL); c->reefer = true; }
                else if (strncmp(a, "maxstack=", 9) == 0) { c->max_stack_t = strtof(a + 9, NULL); }
                else if (strncmp(a, "dest=", 5) == 0) {
                    strncpy(c->dest, a + 5, sizeof(c->dest) - 1);
                    c->dest[sizeof(c->dest) - 1] = '\0';
                } else {
                    fprintf(stderr, "Warning: unknown attribute '%s' for cargo '%s'.\n", a, id);
                }
            }
        }

        ship->cargo_count++;
    }
    fclose(file);

    // Warn about duplicate cargo IDs (each duplicated id reported once).
    for (int i = 0; i < ship->cargo_count; i++) {
        bool already = false;
        for (int k = 0; k < i; k++)
            if (strcmp(ship->cargo[k].id, ship->cargo[i].id) == 0) { already = true; break; }
        if (already) continue;
        for (int j = i + 1; j < ship->cargo_count; j++)
            if (strcmp(ship->cargo[i].id, ship->cargo[j].id) == 0) {
                fprintf(stderr, "Warning: Duplicate cargo ID '%s'.\n", ship->cargo[i].id);
                break;
            }
    }
    return 0;
}
