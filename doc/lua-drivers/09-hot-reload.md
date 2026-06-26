# Step 09 — Hot reload of `$pier/lua/`

**Status:** ✅ done & verified live (incremental — see the update at the bottom)
**Goal:** Edit, add, or remove a `.lua` file in a running ship and have the
drivers update themselves — no restart.

> **Updated to incremental reload.** This page describes the original
> *whole-block* reload (every change reloaded every driver). It was later made
> **incremental** — only the changed/added/removed driver is touched, siblings
> keep running. See the "Incremental reload" section at the bottom.

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

## Incremental reload (update)

The original `_lua_reload` tore down the *entire* Lua block and rebuilt it on any
change — so editing one file reset every driver's state and handles. It's now a
**diff against disk**, so only what actually changed is touched:

- Each driver remembers its source **filename** and **mtime (ns)** (`src_c` /
  `mod_d`, set in `_lua_load_one`).
- On a watcher event, `_lua_reload` snapshots the loaded drivers, scans
  `$pier/lua/`, and reconciles by filename + mtime:
  - **new file** → `_lua_load_one` + `_lua_chain_insert` at its priority slot +
    talk just that driver;
  - **deleted file** → `_lua_chain_remove` exits just that driver;
  - **edited file** (mtime changed) → load the new one, exit the old one;
  - **unchanged** → left completely alone.
- Two tiny chain helpers do the surgery: `_lua_chain_insert` (sorted insert of one
  node) and `_lua_chain_remove` (unlink + `u3_auto_exit` of one node). A failed
  load keeps the old driver running.

So editing a driver no longer disturbs its siblings — their luv sockets, timers,
and Lua state survive — and the in-flight-event edge is scoped to just the one
driver you changed.

### Verified live
Boot drivers A and B (each counting on a luv timer):
```
lua: A: A 1 .. A 6      lua: B: B 1 .. B 6
                        # edit only a.lua
lua: reload: +0 added, ~1 changed, -0 removed
lua: A: A-edited 1 ..   lua: B: B 7, B 8, B 9 ..   # B keeps counting, untouched
                        # drop in c.lua
lua: reload: +1 added, ~0 changed, -0 removed
lua: C: C 1 ..          # A and B keep running
                        # rm b.lua
lua: reload: +0 added, ~0 changed, -1 removed
                        # B stops; A and C keep running
```
With the old whole-block reload, B would have reset to `B 1` on the first edit.

## Next

Step 10 — a live `ctx:plan` round-trip: inject a real, observable event into
Arvo.
