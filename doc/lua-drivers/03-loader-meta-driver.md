# Step 03 — Loader + meta-driver

**Status:** ✅ done & verified (in isolation; wired into boot in step 04)
**Goal:** Turn `.lua` files into live `u3_auto` drivers. Scan a folder, load
each script, expect it to `return` a driver table, wrap each table in a
`u3_auto` whose `talk`/`kick`/`exit` trampoline into Lua, and return them as one
sub-chain sorted by `priority`.

## The driver-table contract

A script must `return` a table:

```lua
return {
  name     = "echo",     -- optional (defaults to filename); for logs
  priority = 10,         -- optional (default 100); lower = earlier in chain
  talk = function(ctx) ... end,            -- optional: startup
  kick = function(ctx, wire, card) ... end,-- effect handler -> truthy if handled
  exit = function(ctx) ... end,            -- optional: teardown
}
```

`wire` and `card` arrive as `u3.noun` userdata (from the step-02 bridge). `kick`
returns truthy to claim the effect, falsy to pass it to the next driver — this
is exactly the C `kick_f` → `c3y`/`c3n` contract.

## Files

| File | Change |
|------|--------|
| `pkg/vere/lua.c` | + `u3_lua_driver` wrapper, `ctx` userdata (`:log`), trampolines, loader |
| `pkg/vere/lua.h` | + `ref_w` on `u3_lua`; declare `u3_lua_init` / `u3_lua_load_dir` |
| `pkg/c3/motes.h` | + `#define c3__lua c3_s3('l','u','a')` |
| `pkg/vere/lua_tests.c` | + loader tests (empty / priority-sort / bad-script) |

## Design notes

### `u3_lua_driver` wrapper
```c
typedef struct _u3_lua_driver {
  u3_auto  car_u;   // first field -> a u3_auto* casts to this
  u3_lua*  lua_u;   // shared bridge state (borrowed)
  c3_w     pri_w;   // priority
  c3_c*    nam_c;   // name (for logs)
  int      tab_r;   // registry ref: driver table
  int      ctx_r;   // registry ref: ctx userdata
} u3_lua_driver;
```
The driver table and its `ctx` are pinned in the Lua **registry** (`luaL_ref`),
so they survive across callbacks and can't be GC'd while the driver lives.

### `nam_m`
All Lua drivers carry the `c3__lua` mote as their `u3_auto.nam_m`. This matters
because the runtime reads `nam_m` back as a cord (`u3r_string`) and raw bytes
for leak-labels / mass reporting — so it must be a valid ≤4-char mote, which a
free-form driver name is not. Per-driver identity is kept in `nam_c` for logs.

### Shared-state lifetime (the tricky bit)
One `lua_State` is shared by all Lua drivers in a pier, but `u3_auto_exit` tears
drivers down one at a time and never frees the chain itself. So `u3_lua.ref_w`
**counts live drivers**: each driver increments it at load, and each driver's
`exit` trampoline decrements it — the **last driver out** calls `u3_lua_halt`
(`lua_close`). If a folder has no loadable scripts, the loader closes the
orphaned state immediately and returns `0`.

### Priority sort
Drivers are collected then **stable-insertion-sorted** by ascending `priority`
(ties keep on-disk scan order), then linked head→tail with `nex_u`; the tail's
`nex_u` is left `0` so step 04 can splice the block into the bigger chain.

### Robustness
A script that fails to load, raises while running, or doesn't return a table is
logged and **skipped** — one bad driver never aborts boot. Non-`.lua` files are
ignored. The folder is created if absent (via `u3_foil_folder`).

### `kick` refcount contract
`_lua_io_kick` owns its `wire`/`card` references (per the `u3_auto` contract)
and `u3z`es them before returning. The nouns handed to Lua are RETAINed
(`u3_lua_push_noun` gains a ref that Lua's GC later releases), so there's no
double-free and no leak.

## Public API (lua.h)

```c
struct _u3_auto* u3_lua_init(struct _u3_pier* pir_u);       // scans $pier/lua/
struct _u3_auto* u3_lua_load_dir(struct _u3_pier*, const c3_c* dir_c); // explicit dir
```
`u3_lua_init` just builds `<pax_c>/lua` and calls `u3_lua_load_dir`. The dir
form (with `pir_u == 0`) is what the tests drive.

## Verification

```sh
ZIG=/home/amadeo/zig-x86_64-linux-0.15.2/zig
$ZIG build lua-test    # exit 0
$ZIG build             # full runtime still builds, exit 0
```

`lua_tests.c` (step-03 cases) writes scripts to temp dirs and checks:
- **empty folder** → `u3_lua_load_dir` returns `0` (and doesn't leak the state);
- **priority sort** → three scripts written out of order on disk load as a
  3-driver chain in priority order; every wrapper carries `c3__lua` and all
  three callbacks; `u3_auto_talk` marks the head live (and `cee`'s `talk` logs
  via `ctx:log`); the head driver's `kick` **handles** a matching wire (`c3y`)
  and **rejects** a non-matching one (`c3n`); `u3_auto_exit` tears the chain
  down cleanly (last-out closes the shared state);
- **bad scripts** → a non-table return and a load-time `error()` are skipped,
  leaving exactly one good driver.

Observed log output during the run:
```
lua: loaded 3 drivers from /tmp/vere-lua-test-XXXXXX
lua: cee: cee talked
lua: boom.lua: run error: [string "boom.lua"]:1: nope
lua: bad.lua: script must `return` a driver table
lua: loaded 1 driver from /tmp/vere-lua-test-XXXXXX
test_lua: ok
```

## Next

Step 04 — call `u3_lua_init(pir_u)` from `u3_auto_init` and splice the returned
block into the chain at a fixed point (right after `fore`), so the drivers
actually load on a real boot. Then step 05 makes `ctx` able to inject events and
do real I/O.
