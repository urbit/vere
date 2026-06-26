# Step 13 — Embed luv: the IO lives in the Lua library

**Status:** ✅ done & verified live
**Goal:** Stop hand-writing an async IO driver inside `lua.c`. Embed **luv** (the
official libuv↔Lua binding — the same one Neovim exposes as `vim.uv`) so the full
libuv API is available to drivers as a Lua library, and delete our per-primitive
trampolines.

## Why

`lua.c` had grown to ~2,990 lines, ~1,350 of which were a hand-rolled IO driver
(timers, UDP/TCP/pipe sockets, DNS, fs, watch, async fs) plus a use-after-free
registry — none of it Lua-specific. luv provides all of that (and more: `spawn`,
signals, tty, fs_poll…) as a maintained, generic library. So the IO should *be*
the library, with `lua.c` reduced to the genuinely Urbit-specific glue.

Result: **`lua.c` 2,989 → 1,558 lines.**

## What drivers see

Every driver has a global `uv` (the luv module) plus the kept `ctx`:

```lua
function d.talk(ctx)
  local t = uv.new_timer()
  t:start(1000, 1000, function() ctx:log("tick") end)

  local srv = uv.new_tcp(); srv:bind("0.0.0.0", 8080)
  srv:listen(128, function() local c = uv.new_tcp(); srv:accept(c)
    c:read_start(function(e, data) if data then c:write(data) end end) end)

  uv.getaddrinfo("example.com", nil, {family="inet"}, function(e, r) ... end)
  uv.fs_open(path, "r", 420, function(e, fd) ... end)            -- async fs
  local w = uv.new_fs_event(); w:start(path, {}, function(e, name) ... end)

  ctx:scry("cy", "base", "/", function(res) ... end)            -- Arvo read
  ctx:plan("d", wire, card)                                     -- Arvo write
  ctx:http("GET", url, nil, function(status, body, hdrs) ... end)  -- HTTPS
end
```

## The integration (the only glue we kept)

**Per-driver private loop.** Each driver's `lua_State` lets luv create its **own**
`uv_loop_t` (we deliberately do *not* call `luv_set_loop`). That matters for
teardown: luv only installs its `loop_gc` cleanup when it owns the loop, and
`loop_gc` walks+closes that state's handles on `lua_close`. So driver isolation
**and** correct handle teardown become luv's job, not ours.

**Driving the child loop from the king loop.** A small embed harness
(`_lua_embed_luv` / `u3_lua_emb`) runs on `u3L`: a `uv_poll` on the child loop's
`uv_backend_fd`, a `uv_prepare` that arms a `uv_timer` to the child's
`uv_backend_timeout`, and a `uv_check` — each runs `uv_run(child, UV_RUN_NOWAIT)`.
The king loop thus wakes and pumps the child loop exactly when the child has an fd
ready or a timer due (no busy-looping). This is the standard "embed one libuv loop
in another" recipe.

**Teardown** (`u3_lua_halt`): stop+close the 4 harness handles (deferred free via
a small refcount), then `lua_close` — luv's `loop_gc` closes the child handles and
`uv_loop_close`s. Verified clean under hot reload.

**Sandboxing:** `uv.run`, `uv.stop`, `uv.loop_close`, `uv.loop_mode` are removed
from the `uv` table so a driver can't hijack/destroy the loop.

## What stayed in the runtime

luv can't do these, so they remain as `ctx` methods:

- `ctx:plan(vane, wire, card)` and `ctx:scry(care, desk, path, fn)` — need the
  pier/serf, not libuv.
- `ctx:http(...)` — libcurl multi (HTTPS/TLS, which luv has no notion of).
- `ctx:wish`, `ctx:log`, `ctx:pier_path`, and the whole **noun bridge**.

The live-driver registry was kept only for `scry`/`http` (their async callbacks);
all the socket/timer/fs liveness code went away with the trampolines.

## Build

`ext/luv/` vendors luv 1.48.0-2 (unity build of `src/luv.c`) against **our**
`ext/libuv` (1.50.0) and `ext/lua` headers; `ext/lua` now also installs its
headers at the include root so luv.h's `<lua.h>` resolves. Wired into the root and
`pkg/vere` build configs next to `lua`.

## Verification

- `zig build` + `zig build lua-test` green (tests updated: removed the socket/fs
  guard tests for the deleted methods; `_test_ctx_guards` now checks plan/scry/http
  raise without a pier).
- **Live fake ship**, all through luv unless noted:
  ```
  lua: tick: live (luv 1.50.0); ... tick 1..15
  lua: net: udp recv 'hello-udp' from 127.0.0.1:39990
  lua: net: tcp server got 'ping'      / tcp client got 'echo:ping'
  lua: net: pipe server got 'ping'     / pipe client got 'pong'
  lua: net: dns localhost -> 127.0.0.1            # luv getaddrinfo
  lua: net: watch fired: watched.txt   / async wrote watched.txt
  lua: http: http GET -> status=200 body='hello-from-http'   # ctx:http, server = luv tcp
  lua: scry: scry %cy base / -> result (cell=true mug=...)   # ctx:scry
  ```
- **Hot reload with luv handles**: driver A (luv timer) → edit → `A exit` (luv
  `loop_gc` closes its handles) → driver B runs → clean shutdown, no crash.

(Notably, luv's `getaddrinfo` resolved `localhost` here, where our hand-rolled
`uv_getaddrinfo` had hung — another point for letting the library own the IO.)

## Net effect

The IO is no longer a driver embedded in a language binding. It's a library the
Lua side calls; `lua.c` is back to the noun bridge, the loader/meta-driver, and a
handful of Urbit-specific `ctx` methods. Examples in `examples/` rewritten to
`uv.*`.
