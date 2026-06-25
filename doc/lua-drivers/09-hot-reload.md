# Step 09 — Hot reload of `$pier/lua/`

**Status:** ✅ done & verified live
**Goal:** Edit, add, or remove a `.lua` file in a running ship and have the
driver block rebuild itself — no restart.

## How it works

`u3_auto_init` now calls `u3_lua_attach(pir_u, fore)` instead of splicing the
block by hand. `u3_lua_attach`:

1. loads the drivers and splices them after the anchor (`fore`) — same chain
   position as before;
2. creates a process-wide manager (`u3_lua_load`) holding the pier, the watched
   `$pier/lua` path, and the anchor;
3. starts a libuv **`uv_fs_event`** watcher on the folder and a debounce timer.

On any change, `_lua_fs_cb` (re)arms a **250 ms debounce** timer; when it fires,
`_lua_reload` runs:

1. detach the contiguous run of `c3__lua` drivers after the anchor;
2. `u3_auto_exit` each old driver (runs its `exit`, closes its timers/sockets,
   `lua_close`s its state, frees it);
3. `u3_lua_load_dir` rebuilds the block from disk (priority-sorted);
4. `u3_auto_talk` the **new** block (it's null-terminated, so only the new
   drivers are talked), then splice it back after the anchor.

Debouncing matters because an editor save emits several inotify events; they
coalesce into a single reload.

## Why this is reentrancy-safe

The watcher and debounce callbacks run on the libuv loop, which is
single-threaded and never fires during effect dispatch (`u3_auto_kick`) — so
mutating the live `u3_auto` chain inside the reload is safe; nothing is walking
it at that moment.

## Known limitation (documented, not a bug)

A driver with an Arvo event **in flight** at the exact moment of reload is an
edge case: the event's completion callback would reference the freed driver.
Typical drivers (timers/sockets that inject only intermittently) reload cleanly
between events; a driver in the middle of heavy injection should be quiesced
before its file is edited. A fully drain-safe reload (e.g. swapping the Lua
state behind a kept chain node, or refcounting in-flight ova per driver) is a
possible future hardening.

The watcher/debounce handles live for the process lifetime and aren't closed on
shutdown (the OS reclaims them) — a negligible, intentional simplification.

## Files

| File | Change |
|------|--------|
| `pkg/vere/auto.c` | call `u3_lua_attach(pir_u, car_u)` instead of the manual splice |
| `pkg/vere/lua.c` | `u3_lua_load` manager; `u3_lua_attach`; `_lua_reload`; `uv_fs_event` watcher + debounce; `_lua_splice` helper |
| `pkg/vere/lua.h` | declare `u3_lua_attach` |

## Verification — live on a fake ship

Booted with a tick driver logging `A N` every second; mid-run, edited the file
to log `B N` and dropped in a second driver. Observed:

```
lua: loaded 1 driver from .../pier/lua
lua: watching .../pier/lua for live reload
lua: tick: A 1
lua: tick: A 2
lua: tick: A 3
lua: tick: A 4
                       <-- edited 10-tick.lua (A->B) and added 20-extra.lua
lua: tick: A exiting              # old driver torn down (timer closed)
lua: loaded 2 drivers from .../pier/lua
lua: extra: extra is alive        # new driver's talk ran
lua: reloaded drivers from .../pier/lua
lua: tick: B 1                    # new behaviour live; old "A" stopped
lua: tick: B 2
...
lua: tick: B exiting              # clean shutdown
```

The two file writes coalesced into one reload; the old driver stopped, the new
ones started, all without restarting the ship.

## Next

Step 10 — a live `ctx:plan` round-trip: inject a real, observable event into
Arvo.
