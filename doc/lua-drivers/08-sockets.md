# Step 08 — UDP + TCP socket I/O primitives

**Status:** ✅ done & verified (unit + live UDP/TCP loopback on a fake ship)
**Goal:** Give Lua drivers real network I/O — UDP datagrams and TCP streams — on
the king's libuv loop, so a driver can talk to the outside world and `ctx:plan`
what it hears into Arvo.

## The `ctx` socket API

| Call | Returns | Meaning |
|------|---------|---------|
| `ctx:udp_open(port[, fn])` | `io` | bind a UDP socket; `fn(data, host, port)` on receive |
| `ctx:tcp_listen(port, fn)` | `io` | listen; `fn(conn)` per accepted connection |
| `ctx:tcp_connect(host, port, fn)` | — | connect; `fn(conn)` on success |

And on a socket/connection handle (`u3.io`):

| Method | Meaning |
|--------|---------|
| `io:send(host, port, data)` | UDP: send a datagram |
| `conn:send(data)` | TCP: write to the stream |
| `io:recv(fn)` | set/replace the receive handler (`fn(data[, host, port])`) |
| `io:close()` | close the socket (idempotent) |

Receive handlers can call `ctx:plan(...)` — that's the whole point: external
bytes become Arvo events.

## Lifetime model (the careful part)

Sockets follow the **same driver-owned, async-close discipline as timers**, with
one extra wrinkle: a socket handle is also exposed to Lua, which could outlive
or misuse it.

- Each socket is a heap `u3_lua_io` linked off its driver (`dvr->io_u`), carrying
  its libuv handle (a union of `uv_udp_t`/`uv_tcp_t`/`uv_stream_t`/`uv_handle_t`).
- `uv_close` is async; the close callback frees the struct and touches **nothing
  else** (lua_State / driver may already be gone).
- The Lua-facing `u3.io` userdata is a **weak handle**: it stores the driver, the
  `u3_lua_io*`, and the socket's `uid_d`. `_io_resolve` validates the handle
  against the driver's *live list* (pointer + uid, so it's ABA-safe) before every
  use, and raises `"socket closed"` otherwise — never dereferencing freed memory.
  This is sound because Lua callbacks only run while the driver (hence its state)
  is alive.
- On driver `exit`, `_lua_close_io` stops + async-closes every socket.
- Writes copy their payload into a `u3_lua_wreq` (flexible-array struct) that
  lives until the send/write completes, then frees itself.

### Bug found & fixed during verification
`c3y == 0` and `c3_calloc` zeroes memory, so a freshly allocated socket's
`dead_o` defaulted to `c3y` (**dead**) — `_io_resolve` rejected every new socket
as "closed". Fix: set `dead_o = c3n` explicitly at all four creation sites. (A
good reminder that loobean fields can't rely on calloc.)

## Files

| File | Change |
|------|--------|
| `pkg/vere/lua.c` | `u3_lua_io`/`u3_lua_wreq` types + `io_u` on the driver; UDP & TCP impl; `u3.io` metatable + methods; `_lua_close_io` wired into exit; `#include <netinet/in.h>,<arpa/inet.h>` |
| `pkg/vere/lua_tests.c` | `_test_socket_guards` |
| `doc/lua-drivers/examples/` | `30-udp-echo.lua`, `40-tcp-echo.lua` |

## Verification

**Unit** (`zig build lua-test`): `udp_open`/`tcp_listen`/`tcp_connect` are
registered and raise cleanly with no pier (no loop) — a driver whose `kick`
`pcall`s all three and returns "all raised" passes.

**Live** (fake ship, fresh binary):

UDP — open, self-send via a timer, receive:
```
lua: udp-echo: listening on udp :39990
lua: udp-echo: sent a datagram to self
lua: udp-echo: recv 'hello-from-lua' from 127.0.0.1:39990
```

TCP — listen, connect to self, accept, echo, read back:
```
lua: tcp-echo: listening on tcp :31338
lua: tcp-echo: client: connected to self, sending ping
lua: tcp-echo: server: accepted a connection
lua: tcp-echo: server: got 'ping', echoing
lua: tcp-echo: client: got 'echo:ping'
```

Both tear down cleanly on shutdown (`exiting`).

### Gotcha
Pick ports that don't collide with other ships on the box — `udp_open` initially
failed with `address already in use` because `31337` was held by a separate
running ship. (Sockets bind on `0.0.0.0`/all interfaces.)

## Notes / follow-ons

- `fd`-style primitives (pipes, arbitrary fds) follow the identical
  init→start→callback pattern; not implemented here but trivial to add.
- No backpressure/flow-control on TCP writes yet (fire-and-forget); fine for
  control-plane volumes, revisit for bulk transfer.

## Next

Step 09 — hot reload: watch `$pier/lua/` and reload the driver block live.
