# Step 07 — Per-driver lua_State isolation

**Status:** ✅ done & verified
**Goal:** Give each Lua driver its own `lua_State`, instead of one shared per
pier, so one driver's globals (or a crash) can't reach another's — and so a
single driver can be reloaded by simply closing its state (sets up step 09).

## The change

Previously the loader booted one shared `lua_State` and ref-counted it across
all drivers (`u3_lua.ref_w`), with the last driver out calling `lua_close`. Now:

- `_lua_load_one` **boots its own `u3_lua` state** (`u3_lua_boot`) per script and
  loads the script into it. Any failure path closes that state immediately.
- `_lua_io_exit` closes the driver's **own** state (`u3_lua_halt`) — no
  ref-counting.
- `u3_lua_load_dir` no longer creates a shared state; it just loads each script.
- `u3_lua.ref_w` is **removed**.

Net: simpler ownership (each driver owns exactly its interpreter) and true
isolation. The noun/ctx metatables are registered per-state by `u3_lua_boot`, so
nothing else changed.

## Trade-offs

- **+** robustness: independent globals; a driver error can't corrupt a sibling.
- **+** clean reload: closing one state reloads one driver (step 09).
- **−** memory: each state carries its own stdlib tables (tens of KB). Fine for
  the handful-of-drivers case; revisit only if someone ships hundreds.
- Drivers can no longer share Lua globals directly — by design. Cross-driver
  communication goes through Arvo (`ctx:plan`), which is the correct channel.

## Files

| File | Change |
|------|--------|
| `pkg/vere/lua.c` | `_lua_load_one` boots/owns a state; `_lua_io_exit` closes it; `u3_lua_load_dir` no shared state |
| `pkg/vere/lua.h` | drop `ref_w`; update `u3_lua` doc |
| `pkg/vere/lua_tests.c` | `_test_isolation` |

## Verification

`zig build lua-test` (and full `zig build`) pass. `_test_isolation` writes two
drivers that each set the **same** global `MARK` to a different value and whose
`kick` returns whether `MARK` is its own. Both pass — under the old shared state,
the second-loaded driver would have clobbered the first's `MARK` and one check
would fail.

## Next

Step 08 — real sockets (`ctx:udp_open`, `ctx:tcp_connect/listen`) on the same
handle-tracking pattern as timers.
