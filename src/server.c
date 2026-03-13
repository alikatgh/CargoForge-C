/*
 * server.c - Minimal JSON-RPC 2.0 HTTP server for CargoForge
 *
 * Single-threaded, zero-dependency server using POSIX sockets.
 * Handles one request at a time. Designed for integration testing
 * and lightweight deployments — not a production HTTP server.
 */

#include "server.h"
#include "libcargoforge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_REQUEST_SIZE (1024 * 1024)  /* 1 MB max request body */
#define RECV_BUF_SIZE    4096

static volatile int server_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    server_running = 0;
}

/* ------------------------------------------------------------------ */
/* MINIMAL JSON FIELD EXTRACTION                                      */
/* ------------------------------------------------------------------ */

/**
 * Extract a JSON string value for a given key.
 * Handles escaped quotes within the value.
 * Returns pointer to start of value (within json), sets *len.
 * Returns NULL if key not found.
 */
static const char *json_find_string(const char *json, const char *key,
                                    size_t *len) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *p = strstr(json, search);
    if (!p) return NULL;

    /* Skip key and colon */
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    if (*p != '"') return NULL;
    p++; /* skip opening quote */

    /* Find closing quote (handle escapes) */
    const char *start = p;
    while (*p && !(*p == '"' && *(p - 1) != '\\'))
        p++;

    *len = (size_t)(p - start);
    return start;
}

/**
 * Extract a JSON integer value for a given key.
 * Returns the integer value, or -1 if not found.
 */
static int json_find_int(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *p = strstr(json, search);
    if (!p) return -1;

    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    return atoi(p);
}

/**
 * Copy a JSON string value (unescaping \n, \t, \\, \").
 * Returns heap-allocated string. Caller must free.
 */
static char *json_extract_string(const char *json, const char *key) {
    size_t len;
    const char *val = json_find_string(json, key, &len);
    if (!val) return NULL;

    char *buf = malloc(len + 1);
    if (!buf) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (val[i] == '\\' && i + 1 < len) {
            i++;
            switch (val[i]) {
                case 'n':  buf[j++] = '\n'; break;
                case 't':  buf[j++] = '\t'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"':  buf[j++] = '"';  break;
                case 'r':  buf[j++] = '\r'; break;
                default:   buf[j++] = '\\'; buf[j++] = val[i]; break;
            }
        } else {
            buf[j++] = val[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

/* ------------------------------------------------------------------ */
/* HTTP RESPONSE HELPERS                                              */
/* ------------------------------------------------------------------ */

static void send_response(int client_fd, int status, const char *body) {
    const char *status_text = (status == 200) ? "OK" : "Bad Request";
    size_t body_len = body ? strlen(body) : 0;

    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, body_len);

    write(client_fd, header, (size_t)header_len);
    if (body && body_len > 0)
        write(client_fd, body, body_len);
}

static void send_jsonrpc_error(int client_fd, int id, int code,
                               const char *message) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%d}",
        code, message, id);
    send_response(client_fd, 200, buf);
}

static void send_jsonrpc_result(int client_fd, int id, const char *result) {
    /* result is already a JSON value/object */
    size_t result_len = result ? strlen(result) : 4;
    size_t buf_size = result_len + 128;
    char *buf = malloc(buf_size);
    if (!buf) {
        send_jsonrpc_error(client_fd, id, -32603, "Internal error");
        return;
    }

    snprintf(buf, buf_size,
        "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%d}",
        result ? result : "null", id);

    send_response(client_fd, 200, buf);
    free(buf);
}

/* ------------------------------------------------------------------ */
/* JSON-RPC METHOD DISPATCH                                           */
/* ------------------------------------------------------------------ */

static void handle_method_optimize(int client_fd, const char *params, int id) {
    char *ship_config = json_extract_string(params, "ship_config");
    char *cargo_manifest = json_extract_string(params, "cargo_manifest");

    if (!ship_config || !cargo_manifest) {
        send_jsonrpc_error(client_fd, id, -32602,
            "Missing required params: ship_config, cargo_manifest");
        free(ship_config);
        free(cargo_manifest);
        return;
    }

    CargoForge *cf;
    if (cargoforge_open(&cf) != CF_OK) {
        send_jsonrpc_error(client_fd, id, -32603, "Failed to create context");
        free(ship_config);
        free(cargo_manifest);
        return;
    }

    int rc = cargoforge_load_ship_string(cf, ship_config);
    if (rc != CF_OK) {
        send_jsonrpc_error(client_fd, id, -32602, cargoforge_errmsg(cf));
        cargoforge_close(cf);
        free(ship_config);
        free(cargo_manifest);
        return;
    }

    rc = cargoforge_load_cargo_string(cf, cargo_manifest);
    if (rc != CF_OK) {
        send_jsonrpc_error(client_fd, id, -32602, cargoforge_errmsg(cf));
        cargoforge_close(cf);
        free(ship_config);
        free(cargo_manifest);
        return;
    }

    rc = cargoforge_optimize(cf);
    if (rc != CF_OK) {
        send_jsonrpc_error(client_fd, id, -32603, cargoforge_errmsg(cf));
        cargoforge_close(cf);
        free(ship_config);
        free(cargo_manifest);
        return;
    }

    const char *json = cargoforge_result_json(cf);
    send_jsonrpc_result(client_fd, id, json);

    cargoforge_close(cf);
    free(ship_config);
    free(cargo_manifest);
}

static void handle_method_validate(int client_fd, const char *params, int id) {
    char *ship_config = json_extract_string(params, "ship_config");
    char *cargo_manifest = json_extract_string(params, "cargo_manifest");

    if (!ship_config) {
        send_jsonrpc_error(client_fd, id, -32602, "Missing param: ship_config");
        free(cargo_manifest);
        return;
    }

    CargoForge *cf;
    cargoforge_open(&cf);

    int ship_ok = (cargoforge_load_ship_string(cf, ship_config) == CF_OK);
    int cargo_ok = 1;
    if (cargo_manifest)
        cargo_ok = (cargoforge_load_cargo_string(cf, cargo_manifest) == CF_OK);

    char result[256];
    snprintf(result, sizeof(result),
        "{\"ship_valid\":%s,\"cargo_valid\":%s,\"valid\":%s}",
        ship_ok ? "true" : "false",
        cargo_ok ? "true" : "false",
        (ship_ok && cargo_ok) ? "true" : "false");

    send_jsonrpc_result(client_fd, id, result);

    cargoforge_close(cf);
    free(ship_config);
    free(cargo_manifest);
}

static void handle_method_version(int client_fd, int id) {
    char result[128];
    snprintf(result, sizeof(result), "\"%s\"", cargoforge_version());
    send_jsonrpc_result(client_fd, id, result);
}

static void dispatch_request(int client_fd, const char *body, int verbose) {
    /* Extract JSON-RPC fields */
    size_t method_len;
    const char *method_raw = json_find_string(body, "method", &method_len);
    int id = json_find_int(body, "id");

    if (!method_raw) {
        send_jsonrpc_error(client_fd, id >= 0 ? id : 0, -32600,
            "Invalid request: missing method");
        return;
    }

    /* Copy method name */
    char method[64];
    size_t copy_len = method_len < sizeof(method) - 1 ? method_len : sizeof(method) - 1;
    memcpy(method, method_raw, copy_len);
    method[copy_len] = '\0';

    if (verbose)
        fprintf(stderr, "[cargoforge] method=%s id=%d\n", method, id);

    /* Find params object (rough extraction) */
    const char *params = strstr(body, "\"params\"");
    if (params) {
        params = strchr(params, '{');
    }

    /* Dispatch */
    if (strcmp(method, "optimize") == 0) {
        if (!params) {
            send_jsonrpc_error(client_fd, id, -32602, "Missing params");
            return;
        }
        handle_method_optimize(client_fd, params, id);
    }
    else if (strcmp(method, "validate") == 0) {
        handle_method_validate(client_fd, params ? params : body, id);
    }
    else if (strcmp(method, "version") == 0) {
        handle_method_version(client_fd, id);
    }
    else {
        send_jsonrpc_error(client_fd, id, -32601, "Method not found");
    }
}

/* ------------------------------------------------------------------ */
/* HTTP REQUEST PARSING                                               */
/* ------------------------------------------------------------------ */

static int handle_client(int client_fd, int verbose) {
    char *buf = malloc(MAX_REQUEST_SIZE);
    if (!buf) return -1;

    size_t total = 0;
    ssize_t n;

    /* Read until we have full headers + body */
    while (total < MAX_REQUEST_SIZE - 1) {
        n = recv(client_fd, buf + total, RECV_BUF_SIZE, 0);
        if (n <= 0) break;
        total += (size_t)n;
        buf[total] = '\0';

        /* Check if we have the full body */
        char *header_end = strstr(buf, "\r\n\r\n");
        if (header_end) {
            size_t header_len = (size_t)(header_end - buf) + 4;

            /* Find Content-Length */
            char *cl = strstr(buf, "Content-Length:");
            if (!cl) cl = strstr(buf, "content-length:");
            size_t content_length = 0;
            if (cl) content_length = (size_t)atoi(cl + 15);

            if (total >= header_len + content_length)
                break;
        }
    }

    if (total == 0) {
        free(buf);
        return 0;
    }

    /* Handle CORS preflight */
    if (strncmp(buf, "OPTIONS", 7) == 0) {
        send_response(client_fd, 200, "");
        free(buf);
        return 0;
    }

    /* Find body */
    char *body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;

    if (!body || *body == '\0') {
        send_jsonrpc_error(client_fd, 0, -32700, "Parse error: empty body");
        free(buf);
        return 0;
    }

    dispatch_request(client_fd, body, verbose);
    free(buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/* SERVER MAIN LOOP                                                   */
/* ------------------------------------------------------------------ */

int cargoforge_serve(int port, int verbose) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 8) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    fprintf(stderr, "CargoForge JSON-RPC server v%s\n", cargoforge_version());
    fprintf(stderr, "Listening on http://0.0.0.0:%d\n", port);
    fprintf(stderr, "Press Ctrl+C to stop\n\n");

    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        if (verbose) {
            fprintf(stderr, "[cargoforge] Connection from %s:%d\n",
                    inet_ntoa(client_addr.sin_addr),
                    ntohs(client_addr.sin_port));
        }

        handle_client(client_fd, verbose);
        close(client_fd);
    }

    close(server_fd);
    fprintf(stderr, "\nServer stopped.\n");
    return 0;
}
