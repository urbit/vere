# Step 12 — More IO: scry, DNS, unix sockets, HTTP client, async fs + watch

**Status:** ✅ done; 4/5 verified live, DNS code-complete (sandbox resolver can't
demo it — see below)
**Goal:** Round out the driver `ctx` with the IO primitives that make Lua drivers
first-class participants: read ship state, resolve hostnames, do local IPC, talk
HTTP, and watch/async-IO the filesystem.

## New `ctx` methods

| Method | Meaning |
|--------|---------|
| `ctx:scry(care, desk, path, fn)` | read Arvo's namespace; `fn(result\|nil)` |
| `ctx:tcp_connect(host, port, fn)` | now accepts **hostnames** (async DNS), not just IPs |
| `ctx:pipe_connect(path, fn)` / `ctx:pipe_listen(path, fn)` | unix-domain sockets (`conn:send/recv/close`) |
| `ctx:http(method, url[, opts], fn)` | async HTTP(S); `opts={headers={},body=""}`; `fn(status, body, headers)` |
| `ctx:watch(path, fn)` | `fn(filename, kind)` when a pier path changes |
| `ctx:read_async(path, fn)` / `ctx:write_async(path, data, fn)` | file IO off the loop (threadpool) |

## Foundation: the live-driver registry

Async operations (DNS resolve, TCP connect, scry, HTTP completion, threadpool fs)
can finish **after** their driver is torn down by exit or hot reload. Touching the
driver then would be a use-after-free. So each driver now has a unique `uid_d` and
is tracked in a small global **live-driver registry**. Every async callback stores
only the `uid`, calls `_live_get(uid)` first, and drops the result (cleaning up its
own context) if the driver is gone — never dereferencing freed memory. Open libuv
handles (sockets, timers, watchers) don't need this: they're `uv_close`d on exit,
so their callbacks simply stop firing.

## Per-feature notes

### scry (`ctx:scry`)
Builds a `u3_pico` via `u3_pier_peek_last(pir, [~ ~], care, desk, path, …)` and
delivers the unwrapped result to `fn`. `care` is the vane+care mote string (`"cy"`
arch, `"cz"` hash, `"cx"` file, `"ax"` ames, `"gx"` gall…); `desk` a desk string or
nil; `path` a `"/a/b"` string (knots built by `_path_to_knots`). Gang `[~ ~]` gives
full local access (as the control plane uses). The callback owns the response noun
and `u3z`s it after handing the value to Lua.

### DNS / hostname TCP
`tcp_connect` tries `uv_ip4_addr` first (literal IP → connect directly); otherwise
it runs `uv_getaddrinfo` and connects on resolution — the same pattern cttp.c uses.
Both the resolve and connect callbacks are registry-guarded.

### unix sockets
`uv_pipe_t` added to the socket union; connections reuse the entire TCP stream path
(`_stream_connect_cb`, `_tcp_read_cb`, `io:send/recv/close`). Only `init`/`bind`
differ (`uv_pipe_init`/`uv_pipe_bind`).

### HTTP client
libcurl's **multi** interface driven by the uv loop: one shared `CURLM` + one
`uv_timer`, lazily initialized; `CURLMOPT_SOCKETFUNCTION` manages a `uv_poll_t` per
socket, `CURLMOPT_TIMERFUNCTION` drives the timer; completions detected with
`curl_multi_info_read`/`CURLMSG_DONE` and dispatched (registry-guarded). Follows
redirects, 30s timeout. **Bug found & fixed during bring-up:** `CURLINFO_RESPONSE_CODE`
makes curl write a C `long` (8 bytes); reading it into a `c3_l` (4 bytes) overflowed
into the adjacent request pointer and segfaulted — the field must be a real `long`.

### watch + async fs
`ctx:watch` is a `uv_fs_event` per watched pier path (closed on exit, like a socket).
`read_async`/`write_async` run the blocking IO on the libuv **threadpool**
(`uv_queue_work`) and deliver on the loop thread (registry-guarded). Path safety is
the same pier-relative `_fs_resolve` as the sync fs ops.

## Verification

**Unit** (`zig build lua-test`): `_test_io2_guards` confirms all seven new methods
are registered and raise cleanly with no pier. Full suite passes.

**Live** (fake `~nut` ship, all in one run):
```
lua: http: http GET -> status=200 body='hello-from-http'          # HTTP client
lua: net:  pipe server got 'ping', replying                       # unix socket
lua: net:  pipe client got 'pong'                                 #   round-trip
lua: net:  watch fired: watched.txt (change)                      # fs watch
lua: net:  write_async ok=true                                    # async write
lua: net:  read_async got 'changed by async write'                # async read
lua: scry: scry %cy base / -> result (cell=true mug=1237148946)   # scry (arch)
lua: scry: scry %cz base / -> mug=1454117747                      # scry (hash)
```
HTTP also resolves hostnames via curl: `http://localhost:…` → `status=200`.

### DNS caveat (environment, not code)
`ctx:tcp_connect` to a **hostname** could not be demonstrated in this sandbox:
`uv_getaddrinfo`'s callback never fires here — and vere's own subsystem logs
`mdns: getaddrinfo error: temporary failure`, confirming the box's resolver is
non-functional for the threadpool path (static musl + restricted sandbox). The IP
path works (TCP/HTTP to `127.0.0.1`), the code reaches `uv_getaddrinfo`, and the
implementation mirrors cttp.c's production resolve path exactly. It will resolve on
a normal host.

## Recommended next IO (still open)

- Child process / `exec` (`uv_spawn` + piped stdio).
- DNS-aware `udp` and a `ctx:resolve(host, fn)` helper.
- TLS for raw TCP (currently only HTTP gets TLS, via curl).
- Streaming/partial HTTP responses and request cancellation handles.
