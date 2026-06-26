# The JSON-RPC server

The `cargoforge serve` subcommand turns the engine into a network service: any program that can send an HTTP POST can now run a full stowage optimisation, stability analysis, or configuration check — without shell access, without linking against `libcargoforge`, and without caring which language it is written in. This lesson walks through how `src/server.c` builds that capability from raw POSIX sockets, why it speaks JSON-RPC 2.0, and what each message looks like on the wire.

---

## Why a server mode?

The CLI is ideal when a person is driving CargoForge interactively. A server is ideal when something else is driving it: a web dashboard, a load-planner built in Python, a mobile app, an automated CI pipeline that validates manifests before every voyage. Rather than shelling out to the binary and scraping text output, those callers POST structured JSON and receive structured JSON in return. The contract is stable, language-agnostic, and testable.

JSON-RPC 2.0 was chosen because it is the simplest call-response protocol with a defined error format. It requires no special library: one `Content-Type: application/json` POST, one JSON object in, one JSON object out.

---

## The public interface

`include/server.h` exports exactly one function:

```c
/* from include/server.h */
int cargoforge_serve(int port, int verbose);
```

It blocks until the process receives `SIGINT` or `SIGTERM`, then returns `0` on a clean shutdown or `-1` if the socket could not be created. Everything else — accept loop, HTTP framing, JSON parsing, method dispatch — is internal to `src/server.c`.

The CLI reaches it via `cargoforge serve --port=8080`.

---

## The request/response shape

JSON-RPC 2.0 defines a small envelope. Every request carries four fields:

| Field | Required | Meaning |
|---|---|---|
| `jsonrpc` | yes | Always `"2.0"` |
| `method` | yes | Which operation to run |
| `params` | method-dependent | Method arguments as a JSON object |
| `id` | yes | Caller-chosen integer; echoed back in the response |

Every response carries:

| Field | Always present | Meaning |
|---|---|---|
| `jsonrpc` | yes | Always `"2.0"` |
| `result` | on success | Method's return value |
| `error` | on failure | Object with `code` and `message` |
| `id` | yes | Echoed from the request |

The header comment in `server.h` shows the `optimize` method end-to-end:

```
POST / HTTP/1.1
Content-Type: application/json

{"jsonrpc":"2.0","method":"optimize","params":{
  "ship_config":"length_m=180\n...",
  "cargo_manifest":"C1 25000 12x2.4x2.6 standard\n..."
},"id":1}
```

The ship config and cargo manifest are passed as **embedded strings** — the same text that would normally live in files, but JSON-escaped and included directly in the params object. The server writes them to temporary files internally via `cargoforge_load_ship_string` / `cargoforge_load_cargo_string` (the `_string` variants in `libcargoforge.c` use `mkstemp` then `unlink`).

---

## Standing up the socket

`cargoforge_serve` in `src/server.c` does standard POSIX socket setup: create, set `SO_REUSEADDR`, bind, listen with a backlog of 8, then enter the accept loop.

```c
/* from src/server.c */
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

addr.sin_family      = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;
addr.sin_port        = htons((uint16_t)port);

bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
listen(server_fd, 8);
```

`SO_REUSEADDR` lets the server restart immediately after it stops without waiting for the kernel's `TIME_WAIT` timeout on the previous socket — a practical necessity during development.

Signal handling is registered before the accept loop:

```c
signal(SIGINT,  signal_handler);
signal(SIGTERM, signal_handler);
signal(SIGPIPE, SIG_IGN);
```

`SIGPIPE` is ignored so that a write to a client that has already disconnected does not kill the process — it just returns `EPIPE`, which the code propagates as a failed `write` and moves on.

The loop itself is one connection at a time:

```c
while (server_running) {
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           &client_len);
    if (client_fd < 0) {
        if (errno == EINTR) continue;   /* interrupted by signal — check server_running */
        perror("accept");
        continue;
    }
    handle_client(client_fd, verbose);
    close(client_fd);
}
```

The `EINTR` check is important: when `SIGINT` arrives, `accept` returns `-1` with `errno == EINTR`. Without the `continue`, the loop would log a spurious error and exit before the flag check at the top.

!!! note "Single-threaded by design"
    The server handles one connection at a time. The source comment is explicit: *"designed for integration testing and lightweight deployments — not a production HTTP server."* For high-throughput use a reverse proxy (nginx, Caddy) in front, or spawn multiple server processes on different ports.

---

## Reading the HTTP request

HTTP/1.1 has a header section ending with `\r\n\r\n` followed by a body whose length is declared in `Content-Length`. `handle_client` reads in 4 KB chunks until it has consumed headers plus the full declared body:

```c
/* from src/server.c */
char *header_end = strstr(buf, "\r\n\r\n");
if (header_end) {
    size_t header_len = (size_t)(header_end - buf) + 4;
    char *cl = strstr(buf, "Content-Length:");
    if (!cl) cl = strstr(buf, "content-length:");
    size_t content_length = 0;
    if (cl) content_length = (size_t)atoi(cl + 15);
    if (total >= header_len + content_length)
        break;
}
```

The maximum request size is 1 MB (`MAX_REQUEST_SIZE = 1024 * 1024`). A ship config plus a cargo manifest for a large vessel fits comfortably within that.

CORS preflight (`OPTIONS`) requests are answered immediately with a 200 and the appropriate `Access-Control-Allow-*` headers — this lets a browser-based dashboard call the server without a proxy.

---

## Minimal JSON extraction

The server has no JSON library dependency. It locates values with three static helper functions:

| Function | What it returns |
|---|---|
| `json_find_string` | Pointer into the raw buffer, sets `*len` |
| `json_find_int` | Integer value, or `-1` if absent |
| `json_extract_string` | Heap-allocated copy, JSON escape sequences decoded |

`json_extract_string` is the workhorse: it allocates a buffer, iterates character by character, and expands `\n`, `\t`, `\\`, and `\"` back to their literal values. The caller owns the memory and must `free` it.

These helpers are intentionally narrow. They work for the fixed schema that CargoForge sends and receives; they are not a general-purpose JSON parser. If you need to add a method whose params contain nested arrays or numbers, you would need to extend them or pull in a small library such as `jsmn` or `cJSON`.

---

## Method dispatch

`dispatch_request` extracts `method` and `id` from the body, then switches on the method name:

```c
/* from src/server.c */
if (strcmp(method, "optimize") == 0) {
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
```

The error code `-32601` is the JSON-RPC 2.0 standard code for "method not found". The other standard codes used by the server:

| Code | Meaning |
|---|---|
| `-32700` | Parse error (empty body) |
| `-32600` | Invalid request (missing `method`) |
| `-32602` | Invalid params (missing required fields) |
| `-32603` | Internal error (allocation failure, engine error) |

---

## The `optimize` method in detail

`handle_method_optimize` is the most important handler — it runs the full pipeline:

```c
/* from src/server.c */
CargoForge *cf;
cargoforge_open(&cf);

cargoforge_load_ship_string(cf, ship_config);
cargoforge_load_cargo_string(cf, cargo_manifest);
cargoforge_optimize(cf);          /* placement + analysis */

const char *json = cargoforge_result_json(cf);
send_jsonrpc_result(client_fd, id, json);

cargoforge_close(cf);
free(ship_config);
free(cargo_manifest);
```

Every step is checked; if any returns a non-`CF_OK` code the handler sends a JSON-RPC error with `cargoforge_errmsg(cf)` as the message, frees all memory, and returns early. No global state is touched: `CargoForge *cf` is fully self-contained, so a failure in one request cannot corrupt the next.

`cargoforge_result_json` returns a lazily cached JSON string built from `AnalysisResult` — the same object described in Lesson 34. The string is owned by `cf` and is freed when `cargoforge_close(cf)` is called, so `send_jsonrpc_result` must use it before that call.

### What the response looks like

```json
{
  "jsonrpc": "2.0",
  "result": {
    "gm_corrected": 1.43,
    "imo_compliant": true,
    "trim": 0.22,
    "heel": 0.8,
    "placed_item_count": 14,
    "total_cargo_weight_kg": 312500.0,
    ...
  },
  "id": 1
}
```

If the ship is overloaded, `perform_analysis` sets `gm` to `NAN`, and `fprint_json_output` emits `null` for all hydrostatic fields — the response is still valid JSON-RPC, not an error.

---

## The `validate` method

`handle_method_validate` runs only the parsing steps — no placement, no analysis. It reports whether the ship config and cargo manifest are syntactically valid:

```c
/* from src/server.c */
int ship_ok  = (cargoforge_load_ship_string(cf, ship_config)    == CF_OK);
int cargo_ok = (cargoforge_load_cargo_string(cf, cargo_manifest) == CF_OK);

snprintf(result, sizeof(result),
    "{\"ship_valid\":%s,\"cargo_valid\":%s,\"valid\":%s}",
    ship_ok  ? "true" : "false",
    cargo_ok ? "true" : "false",
    (ship_ok && cargo_ok) ? "true" : "false");
```

`cargo_manifest` is optional for `validate` — you can check a ship config alone. The `optimize` method requires both.

---

## Sending responses

Two helpers handle the HTTP + JSON-RPC framing:

`send_jsonrpc_result` allocates a buffer sized to the result string, wraps it in the envelope, and calls `send_response`. `send_jsonrpc_error` uses a fixed 1 KB stack buffer (error messages are short).

`send_response` writes HTTP headers followed by the body in two `write` calls:

```c
/* from src/server.c */
char header[512];
snprintf(header, sizeof(header),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %zu\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Connection: close\r\n"
    "\r\n",
    status, status_text, body_len);

write(client_fd, header, (size_t)header_len);
if (body && body_len > 0)
    write(client_fd, body, body_len);
```

`Connection: close` tells the client that the server will close the socket after the response. This simplifies the server — no keep-alive bookkeeping, no idle timeout — at the cost of one TCP handshake per request. For the integration-testing use case the tradeoff is correct.

!!! tip "Testing the server by hand"
    With the server running on port 8080 you can exercise every method from the shell:

    ```bash
    curl -s -X POST http://localhost:8080 \
      -H "Content-Type: application/json" \
      -d '{"jsonrpc":"2.0","method":"version","id":1}'
    ```

    Expected response: `{"jsonrpc":"2.0","result":"3.0.0","id":1}`

---

## Recap

- `cargoforge serve` exposes the engine as a single-threaded JSON-RPC 2.0 HTTP server with no external library dependencies — only POSIX sockets.
- The public interface is one function: `int cargoforge_serve(int port, int verbose)`, declared in `include/server.h`.
- Requests carry `method`, `params`, and `id`; responses carry `result` or `error` plus the echoed `id`.
- Three methods are implemented: `optimize` (full pipeline), `validate` (parse only), and `version`.
- Each request creates and destroys its own `CargoForge *` context via `cargoforge_open` / `cargoforge_close`, so failures are isolated and no global state is shared between requests.
- Signal handling (`SIGINT`/`SIGTERM`) and `SIGPIPE` suppression are essential for graceful shutdown in a socket server.

*Next: [WASM and on-device](45-wasm-and-on-device.md).*
