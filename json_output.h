/*
 * json_output.h - JSON output formatting for web API
 *
 * Provides JSON serialization of ship data, cargo placement,
 * and stability analysis results for web interface integration.
 */

#ifndef JSON_OUTPUT_H
#define JSON_OUTPUT_H

#include "cargoforge.h"

/**
 * print_json_output - Print complete results in JSON format
 *
 * Outputs all ship data, cargo placements, and analysis results
 * as valid JSON to stdout for API consumption.
 *
 * @param ship Ship structure with placed cargo
 * @param result Analysis results
 */
void print_json_output(const Ship *ship, const AnalysisResult *result);

/**
 * escape_json_string - Escape special characters for JSON
 *
 * @param str Input string
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void escape_json_string(const char *str, char *buffer, size_t buffer_size);

#endif /* JSON_OUTPUT_H */
