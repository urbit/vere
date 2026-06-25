# Step 06 — Example drivers + end-to-end fake-ship verification

**Status:** ✅ done & verified on a live ship
**Goal:** Prove the whole feature works on a real Urbit: drop `.lua` files into a
booted pier, (re)start the ship, and watch the drivers load (priority-ordered)
and perform real async I/O.

## Example drivers

Shipped under `doc/lua-drivers/examples/`:

- **`10-tick.lua`** — owns a 1-second repeating libuv timer (`ctx:every`) and
  logs `tick N` each time. The smallest proof a Lua driver is *live* in the
  runtime, doing real async I/O.
- **`20-echo.lua`** — a `kick(ctx, wire, card)` consumer: claims any effect
  whose wire begins with `%echo`, passes everything else through. Demonstrates
  the effect-handling side and the noun-inspection API.

## How a user uses this

```sh
# in a running/booted pier:
cp 10-tick.lua  my-pier/lua/
cp 20-echo.lua  my-pier/lua/
# (re)start the ship — the king scans my-pier/lua/ at boot
urbit my-pier
```

That's the whole UX: drop a file in `$pier/lua/`, restart. No recompile.

## End-to-end run (recorded)

Booted a fake `~bus` with the modified runtime (brass pill + arvo), which
auto-created `$pier/lua/`. Then dropped the example drivers in and resumed.

**Both drivers load, in priority order:**
```
lua: loaded 2 drivers from .../pier/lua
```

**The tick driver does real async I/O inside the live ship** — its libuv timer
fires once per second, calling back into Lua:
```
lua: tick: live — starting a 1s repeating timer
lua: tick: tick 1
lua: tick: tick 2
lua: tick: tick 3
...
lua: tick: tick 15
```

**Clean teardown** on shutdown — the exit trampoline runs and closes the timer:
```
lua: tick: exiting
```

(`20-echo.lua` logs only when it sees an `%echo` effect; none occurred in this
run, which is the expected pass-through behaviour.)

## What this validates, end to end

1. **Vendoring/link** (step 01) — the runtime actually contains and runs Lua.
2. **Bridge** (step 02) — `ctx:log`, noun ops execute on a live loom.
3. **Loader** (step 03) — `$pier/lua/*.lua` → drivers, priority-sorted, robust.
4. **Splice** (step 04) — the block is in the live `u3_auto` chain; boot is
   unaffected.
5. **Real I/O** (step 05) — a `uv_timer_t` on the king loop fires into Lua, and
   tears down cleanly.

## Gotchas hit during verification (for the next person)

- **Resume needs `-t`** in a non-tty environment (`vere: unable to initialize
  terminal`), otherwise the process exits immediately.
- **One fake ship per port.** A leftover `~bus` process holds the mesa/ames UDP
  port; a second instance dies with `mesa: bind: address already in use`. Kill
  the prior instance (via `$pier/.vere.lock`) before resuming.

## Status

All six planned steps are complete and verified. The feature — user-droppable
Lua IO drivers with a numeric priority system and real I/O into the running
ship — works on a real fake ship today.

### Natural follow-ons (not done)
- More `ctx` I/O primitives: UDP/TCP sockets, fds, child processes (each mirrors
  the timer's init→start→callback→`ctx:plan` pattern).
- Hot reload (currently restart-to-reload, by design).
- Per-driver `lua_State` isolation (currently one shared state per pier).
- A demonstrated `ctx:plan` round-trip into a real agent (the injection path is
  implemented and unit-tested; a live agent demo would round it out).
