# Lua IO drivers for Vere — development log

This folder documents an in-progress feature: letting a pier load **IO drivers
written in Lua** from a `$pier/lua/` directory, instead of only the C drivers
compiled into the runtime.

It is a working journal. Each step gets its own `NN-title.md` file describing
*what* changed, *why*, *which files*, and *how it was verified*, so the work can
be reviewed incrementally.

## Goal (one paragraph)

Vere's IO drivers (`behn`, `ames`, `http`, `term`, `unix`, …) are `u3_auto`
structs living in the **king** process, chained together. Arvo emits effects
(cards); each effect is offered to drivers in chain order until one handles it,
and drivers inject events back into Arvo. We want to add a fourth class of
driver authored in Lua: drop a `.lua` file into `$pier/lua/`, and at boot the
king loads it, wraps it in a `u3_auto`, and splices it into the chain. Each Lua
driver declares a numeric `priority`; the Lua drivers form one contiguous block
in the chain, ordered among themselves by that number (lower = first crack).

## Motivation & horizons

The point is to let a **user add IO drivers inside their own ship** — no vere
recompile — and, crucially, to let those drivers eventually perform **real I/O
into the main system**: own real-world handles (sockets, files, timers) and feed
events into Arvo, exactly as the C drivers ames/http/behn do.

That second horizon is why driver scope is *full* (not a pure noun transform):
an IO driver's two powers are (1) handle effects from Arvo via `kick`, and
(2) **inject events into Arvo** via `ctx:plan(wire, card)`. "I/O into the main
system" *is* that injection path. What makes it *real* I/O is giving the Lua
driver access to the king's **libuv event loop** (`u3L`) so it can wait on a
socket/timer/fd asynchronously and `plan` an event when something arrives. The
driver `ctx` API (step 05) is therefore a first-class surface — libuv-backed I/O
primitives that bottom out in `plan` — not an afterthought.

## Design decisions (locked)

| Decision        | Choice                                                      |
|-----------------|-------------------------------------------------------------|
| Lua runtime     | PUC-Lua 5.4.7, vendored under `ext/lua/` (mirrors `ext/lmdb`)|
| Driver scope v1 | *Full* driver: `talk`/`kick`/`exit`, can inject events      |
| Priority model  | Lua drivers = one block, ordered internally by `priority`   |
| Lifecycle       | Scan `$pier/lua/` once at boot; reload = restart            |
| Process         | King-side (drivers live in the king, like all `u3_auto`)    |

## Toolchain note

The project builds with **Zig 0.15.2** (see `INSTALL.md`), located on this
machine at `/home/amadeo/zig-x86_64-linux-0.15.2/zig`. The `/snap/bin/zig`
(0.16.0) is **not** compatible — its `std.Build` API moved `linkLibC` etc. off
`Compile`, so the existing `ext/*/build.zig` files (and ours) fail under it.
Always build with 0.15.2.

## Step index

- [01 — Vendor and link PUC-Lua 5.4](01-vendor-lua.md)
- [02 — Lua↔noun bridge](02-noun-bridge.md)
- [03 — Loader + meta-driver](03-loader-meta-driver.md)
- [04 — Splice the Lua block into the chain](04-splice-into-chain.md)
- [05 — Driver `ctx` API: real I/O](05-ctx-real-io.md)
- [06 — Example drivers + fake-ship verification](06-fake-ship-verification.md)

**Phase 2 — follow-ons:**

- [07 — Per-driver lua_State isolation](07-state-isolation.md)
- [08 — UDP + TCP socket I/O](08-sockets.md)
- [09 — Hot reload of `$pier/lua/`](09-hot-reload.md)
- [10 — Live `ctx:plan` → Arvo injection](10-plan-injection.md)
- [11 — Pier filesystem access](11-pier-filesystem.md)
- [12 — More IO: scry, DNS, unix sockets, HTTP, async fs + watch](12-more-io.md)
- [13 — Embed luv: the IO lives in the Lua library](13-luv-embedding.md)

**All steps complete and verified on a live fake ship.**

> **Architecture note (step 13):** the IO that steps 05–12 hand-wrote in `lua.c`
> was replaced by embedding **luv** (the libuv↔Lua binding). Drivers now do their
> own IO through the global `uv` library; `lua.c` shrank from ~2,990 to ~1,560
> lines. Steps 05–12 document the *capabilities and design*; the *implementation*
> is now luv. The kept-in-runtime bits are `ctx:plan/scry/http/wish/pier_path` and
> the noun bridge.

Example drivers live in [`examples/`](examples/): `10-tick` (luv timer),
`20-echo` (effect handler), `20-net` (luv UDP/TCP/unix/DNS/watch/async-fs),
`50-plan-poke` (inject into Arvo), `70-scry` (read namespace), `80-http` (HTTPS
client + luv TCP server).

## Driver API (current)

**`uv` (global, the luv library)** — full libuv: `uv.new_timer`, `uv.new_udp`,
`uv.new_tcp`, `uv.new_pipe`, `uv.getaddrinfo`, `uv.fs_*` (sync + async),
`uv.new_fs_event`, `uv.spawn`, … (minus `run`/`stop`/`loop_close`).

**`ctx` (Urbit-specific, kept in the runtime):**
`log`, `plan(vane, wire, card)`, `scry(care, desk, path, fn)`,
`http(method, url[, opts], fn)`, `wish(hoon)`, `pier_path()`.

**`noun`** — build/inspect Urbit nouns (`cell`, `atom`, `cord`, `list`, `head`,
`tail`, `eq`, `mug`, `to_int`, `to_string`, …).
