/*
 * server.h - Minimal JSON-RPC 2.0 HTTP server for CargoForge
 *
 * Single-threaded, zero-dependency HTTP server that exposes the
 * libcargoforge API as JSON-RPC methods.
 *
 * Methods:
 *   optimize   — Load ship + cargo, run optimization, return results
 *   validate   — Validate ship + cargo configuration
 *   analyze    — Run stability analysis (no placement)
 *   check_imdg — Check IMDG segregation compliance
 *   version    — Return library version
 *
 * Usage:
 *   cargoforge serve --port=8080
 *
 * Request:
 *   POST / HTTP/1.1
 *   Content-Type: application/json
 *
 *   {"jsonrpc":"2.0","method":"optimize","params":{
 *     "ship_config":"length_m=180\n...",
 *     "cargo_manifest":"C1 25000 12x2.4x2.6 standard\n..."
 *   },"id":1}
 *
 * Response:
 *   {"jsonrpc":"2.0","result":{...},"id":1}
 */

#ifndef SERVER_H
#define SERVER_H

/**
 * Start the JSON-RPC HTTP server.
 * Blocks until terminated (SIGINT/SIGTERM).
 *
 * @param port TCP port to listen on
 * @param verbose Print request logs to stderr
 * @return 0 on clean shutdown, -1 on error
 */
int cargoforge_serve(int port, int verbose);

#endif /* SERVER_H */
