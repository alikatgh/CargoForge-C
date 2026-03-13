/*
 * json_output.h - JSON output formatting
 */

#ifndef JSON_OUTPUT_H
#define JSON_OUTPUT_H

#include "cargoforge.h"

/**
 * fprint_json_output - Write complete results in JSON format to a stream
 *
 * Outputs all ship data, cargo placements, and analysis results
 * as valid JSON. Used by both CLI (stdout) and library (memstream).
 *
 * @param fp Output stream
 * @param ship Ship structure with placed cargo
 * @param result Analysis results
 */
void fprint_json_output(FILE *fp, const Ship *ship, const AnalysisResult *result);

/* Convenience macro for backward compatibility */
#define print_json_output(ship, result) fprint_json_output(stdout, ship, result)

/**
 * escape_json_string - Escape special characters for JSON
 *
 * @param str Input string
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void escape_json_string(const char *str, char *buffer, size_t buffer_size);

#endif /* JSON_OUTPUT_H */
