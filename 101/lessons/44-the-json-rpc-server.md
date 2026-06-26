# The JSON-RPC server

The `cargoforge serve` subcommand turns the engine into a network service: any program that can send an HTTP POST can now run a full stowage optimisation, stability analysis, or configuration check — without shell access, without linking against `libcargoforge`, and without caring which language it is written in. This lesson walks through how [`src/server.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/server.c) builds that capability from raw POSIX sockets, why it speaks JSON-RPC 2.0, and what each message looks like on the wire.

## What this actually means (plain English)

No jargon — here's what the ideas in this lesson *actually* mean, and why they matter.

- **JSON-RPC 2.0** = "a tiny, standardised envelope for remote function calls over HTTP" — CargoForge uses it so that any caller (Python dashboard, mobile app, CI pipeline) can invoke `optimize` or `validate` by POSTing one JSON object and reading back one JSON object, with no bespoke protocol to invent.
- **`cargoforge_serve(int port, int verbose)`** = "the single on/off switch for the whole server" — it is the only function exported from `include/server.h`; everything else (socket setup, HTTP parsing, method dispatch) lives inside `src/server.c` and is invisible to callers.
- **`cargoforge_open` / `cargoforge_close` per request** = "each HTTP request gets its own fresh engine instance, destroyed when the response is sent" — because `CargoForge *cf` is created and freed inside every `handle_method_optimize` call, a crash or bad manifest in one request cannot corrupt the next.
- **`SO_REUSEADDR`** = "let the server restart immediately without waiting for the OS to forget the old socket" — without this flag, restarting the process during development causes a "address already in use" error for up to two minutes while the kernel drains `TIME_WAIT` state.
- **`SIGPIPE` ignored** = "if a client disconnects mid-response, keep running instead of dying" — `write` will return `EPIPE` which the code discards; without `SIG_IGN` the whole server process would be killed by a single impatient client.
- **`cargoforge_load_ship_string` / `cargoforge_load_cargo_string`** = "accept a ship config or cargo manifest as an in-memory string, not a file path" — the `_string` variants use `mkstemp` + `unlink` internally so callers like `handle_method_optimize` can pass JSON-embedded text directly without touching the filesystem themselves.
- **Method dispatch on `-32601`** = "if the caller names a method that doesn't exist, return a standardised error code, not a crash" — `dispatch_request` replies with JSON-RPC error `-32601` ("Method not found") so automated callers can detect and handle unknown methods programmatically.

**Why it matters:** if you get the per-request lifecycle wrong — leaking a `CargoForge *cf`, ignoring `SIGPIPE`, or forgetting to free `cargoforge_result_json`'s string before calling `cargoforge_close` — one bad request can crash or corrupt the server for every subsequent caller. The isolation guarantees in `handle_method_optimize` are what make the single-threaded design safe to expose over a network.

## The mental model 🧠

You'll forget the protocol details — hold THIS picture instead:

> A hotel concierge desk. Any guest (Python script, mobile app, CI pipeline) walks up and hands the concierge a slip of paper: *"optimize — here's the ship config and the cargo manifest."* The concierge looks up the right specialist (`handle_method_optimize`), hands them the slip, waits while they spin up a fresh engine (`cargoforge_open`), run the numbers, and write the result back on a new slip. The concierge hands the result slip to the guest, tears up the working copies, and closes the specialist's folder (`cargoforge_close`). The next guest gets a completely fresh specialist — nothing from the previous request survives.

That "fresh specialist per guest" pattern is exactly what `handle_method_optimize` enforces: `CargoForge *cf` is created and destroyed inside a single function call. A bad manifest from one caller cannot poison the `AnalysisResult` seen by the next. The concierge desk itself (`cargoforge_serve`) never holds cargo state — it only holds the open socket and the running flag.

The slip of paper is the JSON-RPC envelope: `method` tells the concierge which specialist to summon, `params` carries the ship and cargo text, and `id` is the guest's ticket number echoed back so they can match the answer to their question.

<svg viewBox="0 0 620 310" role="img" xmlns="http://www.w3.org/2000/svg"
  style="max-width:600px;width:100%;height:auto;display:block;margin:1.8rem auto;font-family:var(--md-text-font,inherit);color:var(--md-default-fg-color)">
  <title>JSON-RPC request dispatch flow in CargoForge server.c</title>
  <desc>Diagram showing an HTTP POST travelling through cargoforge_serve, handle_client, dispatch_request, and into one of three method handlers (optimize, validate, version), with the optimize handler calling cargoforge_open, cargoforge_optimize, and cargoforge_close before sending the JSON-RPC response back to the caller.</desc>

  <!-- caller box -->
  <rect x="10" y="120" width="100" height="44" rx="5" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <text x="60" y="137" text-anchor="middle" font-size="11" fill="currentColor" font-weight="600">Caller</text>
  <text x="60" y="152" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.65">HTTP POST</text>

  <!-- arrow: caller → serve -->
  <line x1="110" y1="142" x2="148" y2="142" stroke="currentColor" stroke-width="1.3"/>
  <polygon points="148,138 156,142 148,146" fill="currentColor"/>
  <text x="133" y="134" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.6">JSON-RPC</text>

  <!-- cargoforge_serve box -->
  <rect x="156" y="108" width="120" height="68" rx="5" fill="none" stroke="#12A594" stroke-width="1.8"/>
  <text x="216" y="126" text-anchor="middle" font-size="10" fill="#12A594" font-weight="700">cargoforge_serve</text>
  <text x="216" y="141" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">accept() loop</text>
  <text x="216" y="155" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">handle_client()</text>
  <text x="216" y="169" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.75">dispatch_request()</text>

  <!-- arrow: serve → dispatch -->
  <line x1="276" y1="142" x2="314" y2="142" stroke="currentColor" stroke-width="1.3"/>
  <polygon points="314,138 322,142 314,146" fill="currentColor"/>

  <!-- dispatch box -->
  <rect x="322" y="60" width="108" height="166" rx="5" fill="none" stroke="currentColor" stroke-width="1.4"/>
  <text x="376" y="79" text-anchor="middle" font-size="10" fill="currentColor" font-weight="600">method?</text>

  <!-- three method labels inside dispatch -->
  <rect x="332" y="88" width="88" height="24" rx="3" fill="#12A594" fill-opacity="0.12" stroke="#12A594" stroke-width="1"/>
  <text x="376" y="104" text-anchor="middle" font-size="9.5" fill="#12A594" font-weight="600">optimize</text>

  <rect x="332" y="120" width="88" height="24" rx="3" fill="none" stroke="currentColor" stroke-width="1" stroke-opacity="0.4"/>
  <text x="376" y="136" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">validate</text>

  <rect x="332" y="152" width="88" height="24" rx="3" fill="none" stroke="currentColor" stroke-width="1" stroke-opacity="0.4"/>
  <text x="376" y="168" text-anchor="middle" font-size="9.5" fill="currentColor" opacity="0.7">version</text>

  <rect x="332" y="184" width="88" height="24" rx="3" fill="none" stroke="#D05663" stroke-width="1"/>
  <text x="376" y="200" text-anchor="middle" font-size="9.5" fill="#D05663">−32601 error</text>

  <!-- arrow: optimize → engine column -->
  <line x1="430" y1="100" x2="468" y2="100" stroke="#12A594" stroke-width="1.3"/>
  <polygon points="468,96 476,100 468,104" fill="#12A594"/>

  <!-- engine column -->
  <rect x="476" y="58" width="128" height="36" rx="4" fill="none" stroke="#12A594" stroke-width="1.4"/>
  <text x="540" y="73" text-anchor="middle" font-size="9.5" fill="#12A594" font-weight="600">cargoforge_open()</text>
  <text x="540" y="87" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.65">fresh CargoForge *cf</text>

  <rect x="476" y="102" width="128" height="36" rx="4" fill="none" stroke="#12A594" stroke-width="1.4"/>
  <text x="540" y="117" text-anchor="middle" font-size="9.5" fill="#12A594" font-weight="600">cargoforge_optimize()</text>
  <text x="540" y="131" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.65">placement + analysis</text>

  <rect x="476" y="146" width="128" height="36" rx="4" fill="none" stroke="#12A594" stroke-width="1.4"/>
  <text x="540" y="161" text-anchor="middle" font-size="9.5" fill="#12A594" font-weight="600">cargoforge_close()</text>
  <text x="540" y="175" text-anchor="middle" font-size="8.5" fill="currentColor" opacity="0.65">frees cf + result JSON</text>

  <!-- connector lines between engine boxes -->
  <line x1="540" y1="94" x2="540" y2="102" stroke="#12A594" stroke-width="1.2"/>
  <line x1="540" y1="138" x2="540" y2="146" stroke="#12A594" stroke-width="1.2"/>

  <!-- response arrow back to caller -->
  <line x1="476" y1="260" x2="60" y2="260" stroke="currentColor" stroke-width="1.3" stroke-dasharray="5,3" opacity="0.6"/>
  <polygon points="64,256 56,260 64,264" fill="currentColor" opacity="0.6"/>
  <text x="268" y="253" text-anchor="middle" font-size="9" fill="currentColor" opacity="0.6">JSON-RPC response  {"jsonrpc":"2.0","result":{...},"id":1}</text>

  <!-- downward arrow from cargoforge_close to response line -->
  <line x1="540" y1="182" x2="540" y2="260" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>
  <line x1="540" y1="260" x2="477" y2="260" stroke="currentColor" stroke-width="1.2" opacity="0.5"/>

  <!-- response arrow up on caller side -->
  <line x1="60" y1="260" x2="60" y2="164" stroke="currentColor" stroke-width="1.3" stroke-dasharray="5,3" opacity="0.6"/>
  <polygon points="56,168 60,160 64,168" fill="currentColor" opacity="0.6"/>
</svg>

---

## Why a server mode?

The CLI is ideal when a person is driving CargoForge interactively. A server is ideal when something else is driving it: a web dashboard, a load-planner built in Python, a mobile app, an automated CI pipeline that validates manifests before every voyage. Rather than shelling out to the binary and scraping text output, those callers POST structured JSON and receive structured JSON in return. The contract is stable, language-agnostic, and testable.

JSON-RPC 2.0 was chosen because it is the simplest call-response protocol with a defined error format. It requires no special library: one `Content-Type: application/json` POST, one JSON object in, one JSON object out.

---

## The public interface

[`include/server.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/server.h) exports exactly one function:

```c
/* from include/server.h */
int cargoforge_serve(int port, int verbose);
```

It blocks until the process receives `SIGINT` or `SIGTERM`, then returns `0` on a clean shutdown or `-1` if the socket could not be created. Everything else — accept loop, HTTP framing, JSON parsing, method dispatch — is internal to [`src/server.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/server.c).

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

`cargoforge_serve` in [`src/server.c`](https://github.com/alikatgh/CargoForge-C/blob/main/src/server.c) does standard POSIX socket setup: create, set `SO_REUSEADDR`, bind, listen with a backlog of 8, then enter the accept loop.

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
- The public interface is one function: `int cargoforge_serve(int port, int verbose)`, declared in [`include/server.h`](https://github.com/alikatgh/CargoForge-C/blob/main/include/server.h).
- Requests carry `method`, `params`, and `id`; responses carry `result` or `error` plus the echoed `id`.
- Three methods are implemented: `optimize` (full pipeline), `validate` (parse only), and `version`.
- Each request creates and destroys its own `CargoForge *` context via `cargoforge_open` / `cargoforge_close`, so failures are isolated and no global state is shared between requests.
- Signal handling (`SIGINT`/`SIGTERM`) and `SIGPIPE` suppression are essential for graceful shutdown in a socket server.

*Next: [WASM and on-device](45-wasm-and-on-device.md).*
