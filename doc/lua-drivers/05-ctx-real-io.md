# Step 05 — Driver `ctx` API: real I/O

**Status:** ✅ done & verified (unit + live fake-ship)
**Goal:** Give a Lua driver the ability to act on the world and on the ship —
inject events into Arvo, and own real async I/O handles in the king's event
loop. This is the payoff for "real I/O into the main system".

## The `ctx` object

Every driver callback receives a `ctx` (a `u3.ctx` userdata bound to the
driver). Methods:

| Method | Effect |
|--------|--------|
| `ctx:log(str)` | write a runtime log line tagged with the driver name |
| `ctx:plan(vane, wire, card)` | **inject an event into Arvo** |
| `ctx:wish(str)` | evaluate a hoon expression king-side (ivory kernel) → noun |
| `ctx:after(ms, fn)` | call `fn(ctx)` once after `ms` (one-shot libuv timer) |
| `ctx:every(ms, fn)` | call `fn(ctx)` every `ms` (repeating libuv timer) |

### `ctx:plan(vane, wire, card)` — into the main system
`plan` is the bridge from the outside world into the running ship. `vane` is a
short string (the target, e.g. `"b"` behn, `"g"` gall) that becomes the first
knot of the wire; `wire` must be a path (a non-empty cell); `card` is the
`[%tag data]` effect. It maps directly onto the runtime's own
`u3_auto_plan(car_u, u3_ovum_init(0, tar, wir, cad))` — the exact call the C
drivers use. (`u3_ovum_init` consumes its noun args, and reads `u3h(wire)` for
the spinner label, which is why `wire` must be a non-empty cell.)

Because `u3_auto_plan` is safe to call from a libuv callback (behn does exactly
this), a driver can wait on real I/O — a timer, a socket — and `ctx:plan` the
result into Arvo when it arrives. That closes the loop:

```
real-world event  ->  libuv handle fires  ->  Lua callback  ->  ctx:plan  ->  Arvo
```

### libuv timers — the first real async I/O
`ctx:after` / `ctx:every` create real `uv_timer_t` handles on the king loop
(`u3L`). They are the first concrete I/O primitive; the same pattern (init handle
→ start → callback trampolines into Lua → optionally `ctx:plan`) extends to UDP
/ TCP / fds in future steps.

## Memory & lifetime (the careful bits)

- **plan refcounts:** `vane`/`wire`/`card` are built/coerced to owned (+1) refs
  and handed to `u3_ovum_init`, which consumes them — no leak, no double free.
  The pier guard runs *first*, so an error path builds no nouns.
- **timer lifetime:** each timer is a `u3_lua_timer` linked off its driver.
  `uv_close` is async, so the handle frees itself in its close callback, which
  touches **only** the timer struct — never the `lua_State` or the driver, both
  of which may already be gone. A one-shot timer disposes itself right after
  firing. On driver `exit`, `_lua_close_timers` stops + closes every timer
  (releasing its Lua fn ref while the state is still open) *before* the driver
  struct is freed and the shared state is closed.
- **no-pier guard:** `plan`/`after`/`every` raise a clean Lua error when there's
  no pier (the unit-test context), rather than dereferencing null.

## Files

| File | Change |
|------|--------|
| `pkg/vere/lua.c` | `u3_lua_timer` type + `tim_u` field; `_ctx_plan`/`_ctx_wish`/`_ctx_after`/`_ctx_every`; timer fire/close/unlink; `_lua_close_timers`; registered in `_ctx_methods`; called from `_lua_io_exit` |
| `pkg/vere/lua_tests.c` | `_test_ctx_guards` |

## Verification

**Unit** (`zig build lua-test`): a driver whose `talk` calls `ctx:after` and
whose `kick` calls `ctx:plan` is loaded with no pier; both raise cleanly
(caught by the trampolines' `pcall`, logged), `kick` returns `c3n`, and teardown
is clean. Observed:
```
lua: io: talk error: [string "io.lua"]:2: ctx timer: unavailable without a pier
lua: io: kick error: [string "io.lua"]:4: ctx:plan: unavailable without a pier
```

**Live** (fake ship, see step 06): the `ctx:every` timer fires once per second
inside the running ship, and `_lua_close_timers` tears it down cleanly on exit.

## Next

Step 06 — package the example drivers and record the end-to-end fake-ship run.
