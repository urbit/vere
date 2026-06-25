# Step 04 — Splice the Lua block into the driver chain

**Status:** ✅ done (boot smoke-test in progress / see verification)
**Goal:** Actually call the loader on a real boot and insert the loaded Lua
drivers into the live `u3_auto` chain, so dropping a `.lua` file into
`$pier/lua/` makes a driver run.

## The change

`u3_auto_init()` (`pkg/vere/auto.c`) builds the fixed built-in chain by
prepending each driver, so the returned head is `fore` and the order is:

```
fore -> term -> unix -> cttp -> http -> ames -> mesa -> lick -> conn -> behn -> hind
```

After that chain is built, we call `u3_lua_init(pir_u)` (step 03) and splice its
returned sub-chain **right after the head (`fore`)**:

```c
u3_auto* lua_u = u3_lua_init(pir_u);
if ( lua_u && car_u ) {
  u3_auto* tal_u = lua_u;
  while ( tal_u->nex_u ) tal_u = tal_u->nex_u;  // find block tail
  tal_u->nex_u = car_u->nex_u;   // block tail -> term (former 2nd)
  car_u->nex_u = lua_u;          // fore -> block head
}
```

Resulting chain:

```
fore -> [ lua:prio-lowest .. lua:prio-highest ] -> term -> unix -> ... -> hind
```

`pkg/vere/auto.c` also gains `#include "lua.h"`.

## Why after `fore`

`fore` ("first events") stays the absolute head. Putting the Lua block next
means user drivers get first crack at effects ahead of the system drivers
(`term`, `unix`, `ames`, …), while still yielding to anything `fore` must handle
first. Because a Lua `kick` returns falsy for wires it doesn't recognize, the
effect simply falls through to the system drivers — so a well-behaved Lua driver
that matches only its own wire prefix never interferes with built-ins. It's a
single link site, trivial to move if a different position is wanted later.

## pir_u on Lua drivers

Built-in drivers get `car_u->pir_u` set by `_auto_link`; the Lua block bypasses
that, so `_lua_load_one` now sets `car_u->pir_u` from the shared state's pier
pointer (`lua_u->pir_u`, which is `0` under tests). This is what `ctx:plan` in
step 05 needs to enqueue events and spin the pier.

## Files

| File | Change |
|------|--------|
| `pkg/vere/auto.c` | `#include "lua.h"`; splice block after `fore` in `u3_auto_init` |
| `pkg/vere/lua.c`  | `_lua_load_one` sets `car_u->pir_u = lua_u->pir_u` |

## Verification

```sh
ZIG=/home/amadeo/zig-x86_64-linux-0.15.2/zig
$ZIG build          # full runtime builds, exit 0
$ZIG build lua-test # loader/bridge tests still pass, exit 0
```

End-to-end **fake-ship boot** smoke test — boot `--fake bus` with the brass pill
(`88c6173…`) and arvo source, using our built binary:

```
$URB --lite-boot --daemon --fake bus --bootstrap ./brass.pill \
     --arvo ./urbit/pkg/arvo --pier ./pier
```

Result: **boot succeeds** (bootstrap completes, gall installs all agents, kiln
boots, epoch 0 loads) and **`./pier/lua/` is auto-created** (by `u3_foil_folder`
inside `u3_lua_init`). With the folder empty, the loader returns `0` and stays
quiet — no drivers, no noise, no boot regression. This confirms the splice is
safe on a real ship. A meaningful driver dropped into that folder is exercised
in step 06.

## Next

Step 05 — make `ctx` capable of real work: `ctx:plan(wire, card)` to inject
events into Arvo, and libuv-backed I/O (timer → socket) so a Lua driver can
perform real I/O and feed the main system.
