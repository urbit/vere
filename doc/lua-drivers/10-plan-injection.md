# Step 10 — Live `ctx:plan` → Arvo injection

**Status:** ✅ done & verified live (delivery to the target vane proven via stack trace)
**Goal:** Demonstrate the other half of "real I/O into the main system": a Lua
driver injecting a real event into Arvo with `ctx:plan`, on a running ship, and
confirming the event reaches the target vane.

## What `ctx:plan` does (recap)

`ctx:plan(vane, wire, card)` enqueues an event via the same path the C drivers
use: `u3_auto_plan(car_u, u3_ovum_init(0, tar, wire, card))`. The runtime hands
it to the serf as `[[vane wire] card]`; Arvo routes the `card` to the named
`vane`. So a Lua driver that hears something from the outside world (a socket, a
timer, a file) can poke the running ship with it.

## The example

`doc/lua-drivers/examples/50-plan-poke.lua` injects, 2s after going live, a dill
`%flog %text` — the kernel's "print a line" path:

```lua
local tape = noun.tape("hello from a lua driver, via ctx:plan!")
local task = noun.cell(noun.cord("text"), tape)   -- [%text tape]
local card = noun.cell(noun.cord("flog"), task)   -- [%flog [%text tape]]
ctx:plan("d", noun.list("lua"), card)             -- -> dill (vane %d)
```

## Verification — live on a fresh `~nut` ship

Booted a fresh fake `~nut` (a distinct ship from any `~bus` on the box, to avoid
port collisions), then dropped the driver in via **hot reload**:

```
lua: loaded 1 driver from .../pier/lua
lua: poke: injecting a dill %flog %text into Arvo in 2s
lua: reloaded drivers from .../pier/lua
lua: poke: injected           # ctx:plan returned with no error; ship stayed up
```

The injection runs cleanly and the ship stays stable across repeated injections.
A *valid* card (`%flog %text`) is handled **silently** by dill — and on a headless
test ship (no attached terminal) its console output isn't visible — so to prove
the event genuinely reaches the target vane, inject a **deliberately invalid**
card and read Arvo's crash report:

```lua
ctx:plan("d", noun.list("lua","probe"), noun.cell(noun.cord("zzbogus"), 0))
```

Result on the live ship:

```
lua: poke: injected an invalid dill task
lua: bail 1
[%poke %zzbogus]
bar-stack=~[~[//lua/probe]]                 <-- OUR injected wire
/sys/vane/dill/hoon:<[258 3].[417 13]>      <-- faulted INSIDE dill (vane %d)
/sys/vane/dill/hoon:<[259 3].[417 13]>
...
[%poke %crud]                               <-- Arvo wrapped the crash; ship lived
lua: %zzbogus event on /lua/probe failed
```

This is unambiguous end-to-end proof:

- the event **reached Arvo** and was **routed to dill** (the stack trace is in
  `/sys/vane/dill/hoon`),
- it carried **our wire** (`//lua/probe` in the bar-stack),
- Arvo handled the bad task with its normal `%crud` recovery and the ship kept
  running.

So: a *valid* card is delivered and processed by the vane silently; the *invalid*
card proves the delivery path by faulting inside the target vane with our wire in
the trace. The Lua → `ctx:plan` → `u3_auto` → serf → Arvo → vane round trip works.

## Notes

- `u3_ovum_init` reads `u3h(wire)` for the spinner label, so `wire` must be a
  non-empty path — `ctx:plan` enforces this (raises otherwise).
- To get *visible* output from a valid injection, target a vane/agent that slogs
  or run the ship with a terminal/agent that surfaces the effect (e.g. poke a
  gall agent that prints). The mechanism is identical; only the observability
  differs.
- A driver combining a socket (step 08) with `ctx:plan` is the full pattern:
  external bytes in → event into Arvo.

## Status

All ten steps (six core + four follow-ons) are complete and verified on live
fake ships. The feature set: user-droppable Lua IO drivers with numeric priority,
isolated per-driver interpreters, real timer/UDP/TCP I/O, event injection into
Arvo, and hot reload.
