# Pier-level baseline — boot / snapshot / replay / warm jets (2026-06-24)

The integration half of the baseline: the paths the standalone
`benchmarks.c` harness can't reach (a real jetted kernel, the event
log, snapshots). Captured with `pier-bench.sh` on the same host
(Ryzen 7800X3D), `urbit` built from commit `02b4ed7db0` at ReleaseFast.

```
== Tier 1: warm jet dispatch (urbit eval) ==
  eval startup (add 2 3)        415 ms   (ivory load + compile; fixed cost)
  jetted dec 1M                 558 ms   (~143 ms compute → jetted dec ×1,000,000)
  jetted ack(3,7)               549 ms   (~134 ms compute → jetted ack(3,7), ~690k calls)

== Tier 2: boot / snapshot / replay (fake ~bus, brass pill) ==
  boot (-F bus, -x)            31.6 s
  snapshot size (.urb/chk)      99M
  event log size (.urb/log)     14M
  replay (--replay, -x)         0.4 s
```

## Tier 1 — warm jet dispatch (`urbit eval`)

`urbit eval` loads the **embedded ivory pill**, so the jetted Hoon kernel is
live (2178 jets) and `dec`/`add`/etc. dispatch to their C jets. This is the
**warm** counterpart to `benchmarks.c`'s `slam`/`dec`/`ack`, which run against
*unregistered* batteries (cold → Nock fallback).

- Subtract the ~415 ms fixed cost (ivory cue + compile + slam) to isolate
  compute. The headline: **jetted `ack(3,7)` ≈ 134 ms for ~690k calls ≈ 194
  ns/call**, versus the cold C `ack(3,5)` at ~107 ms / ~42k calls ≈ **2.5
  µs/call** — i.e. warm dispatch + native jet bodies are **~13× faster per
  call** here. (Not apples-to-apples — the eval loop also interprets the trap
  recursion — but it brackets the warm/cold gap the jets buy.)
- This is the right harness to measure **jet-dispatch** report items
  (J3–J5: warm-hump treap, fused cold+warm lookup, `_cj_fine`), since they only
  fire on real registered jets.

## Tier 2 — boot, snapshot, replay

- **Boot ≈ 31.6 s.** This is a **brass** pill (compile-the-kernel-from-source);
  almost all of it is Hoon compilation, not runtime. A **solid** pill
  (snapshot-restore) boots in seconds — use brass only to stress the compiler /
  full lifecycle. Boot installs 2178 jets and writes the first snapshot.
- **Snapshot ≈ 99 MB** (`.urb/chk` image) for a freshly-booted `%base`. Written
  synchronously on the main thread — directly the target of report **§4 / P1**
  (async/fork snapshot) and **P2** (double write-amplification).
- **Replay ≈ 0.4 s** — but this is **incremental** replay from the latest
  snapshot (only the events after it; here event 21). A **full** from-scratch
  replay re-runs the entire boot lifecycle (~the 31.6 s above), which is what a
  crash with a stale snapshot costs — motivating report **P12** (volume-based
  snapshot cadence) and **K5** (batch replay under one soft frame).
- The replay also triggered **`disk: rolling to epoch 21` → `loom: image backup
  complete`**: a full byte-for-byte copy of the 99 MB image. That is report
  finding **P8** (epoch backup copies the whole image; use reflink /
  `copy_file_range`) observed live.

## Cross-reference to `PERFORMANCE_REPORT.md`

| observed | report finding |
|---|---|
| 99 MB snapshot written synchronously at boot | **P1** (async/fork save), **P2** (double-write) |
| epoch roll = full 99 MB image byte-copy | **P8** (reflink/copy_file_range backup) |
| full replay ≈ re-run boot; incremental ≈ fast | **P12** (snapshot cadence), **K5** (batch replay) |
| warm jet dispatch now measurable (≈194 ns/call) | **J3–J5** (jet dispatch hot path) |

## Reproduce

```sh
# Tier 1 only (no network):
doc/benchmarks/pier-bench.sh

# Tier 1 + Tier 2 (needs a brass pill + arvo; see boot-fake-ship.sh):
REV=88c6173048d61ebd86455f0c1a8ce8f8099cbe01
curl -sLJ -o brass.pill https://github.com/urbit/urbit/raw/$REV/bin/brass.pill
curl -sLJ https://github.com/urbit/urbit/archive/$REV.tar.gz | tar xz
PILL=$PWD/brass.pill ARVO=$PWD/urbit-$REV/pkg/arvo doc/benchmarks/pier-bench.sh
```

## Caveats

- Boot/replay wall times include process startup, loom map (2 GB), and (boot)
  Hoon compilation; they are coarse integration numbers, not microbenchmarks.
- The brass-boot lifecycle is essentially one large event, so the event log is
  small and incremental replay is trivial. For meaningful replay-scaling data,
  inject a workload (many events) before measuring replay — a natural next step.
- `urbit eval` re-pays the ~415 ms ivory/compile cost every invocation; for
  steady-state jet throughput, measure inside a longer-running expression or a
  booted ship rather than across many `eval` calls.
