# Vere benchmarking & profiling

This directory holds the performance baseline for the optimization work tracked
in `PERFORMANCE_REPORT.md` (repo parent dir). The goal is a **stable, recorded
reference** captured before any runtime changes, plus a reproducible way to
re-measure and to profile with Tracy.

## Files

- `baseline-2026-06-24.md` — the recorded baseline (provenance + 10-run table).
- `run-benchmarks.sh` — reproducible harness: builds ReleaseFast, runs N times,
  prints a min/median/mean/max/stdev table. Does **not** touch the runtime.
- `raw/` — raw per-run output captured for the baseline.

## Toolchain

The project pins **Zig 0.15.2**. Zig 0.16+ changes the `build.zig.zon` manifest
format and will fail with `missing top-level 'fingerprint' field` /
`expected enum literal`. Get 0.15.2 from <https://ziglang.org/download/0.15.2/>.

## Reproduce the timing baseline

```sh
# from repo root; pass a path to zig 0.15.2 if it isn't on PATH
doc/benchmarks/run-benchmarks.sh 10 /path/to/zig
```

The suite (`pkg/vere/benchmarks.c`) is a standalone executable with its own
`main()`; `zig build benchmarks` both builds and runs it once. Output is
millisecond-granularity (`gettimeofday`); phases under ~1 ms are not meaningful.

## Profiling

### Tracy (in-tree, no kernel privileges; preferred for runtime internals)

Tracy is already wired into the build (`-Dtracy`) and into `pkg_noun`
(`pkg/noun/build.zig` parses `-DTRACY_ENABLE` and links the Tracy client +
include path), so files under `pkg/noun` (e.g. `nock.c`, `allocate.c`,
`serial.c`) and the benchmark exe can carry Tracy zones. Zones compile to
**nothing** when `-Dtracy` is off, so they do not affect the baseline.

Instrumentation uses the zero-cost wrappers in `pkg/noun/tracy.h`
(`u3_tc_zone` / `u3_tc_zone_named` / `u3_tc_zone_end` / `u3_tc_frame` /
`u3_tc_plot` / `u3_tc_msg`). Zones in place so far:

| zone | file | what |
|---|---|---|
| `bench:jam` / `bench:cue` / `bench:cue_soft` / `bench:edit` | `pkg/vere/benchmarks.c` | outer group spans + one frame each |
| `u3s_jam_fib`, `u3s_jam_xeno` | `pkg/noun/serial.c` | jam entry points |
| `u3s_cue`, `u3s_cue_xeno` | `pkg/noun/serial.c` | cue entry points |
| `u3n_nock_on` | `pkg/noun/nock.c` | Nock eval entry |

The group zones nest the runtime entry-point zones in the Tracy timeline.
This is a starting set — the next layer (per the perf report's Phase 0) is
zones inside the bytecode dispatch loop (`_n_burn`), jet dispatch, the
allocator fast/slow paths, and snapshot save. Add them with the same wrappers
as the corresponding code is optimized, so each zone lands with its change.

Verified: with `-Dtracy` **off**, the benchmark timings are statistically
identical to the pristine baseline (every phase within its recorded stdev),
confirming the zones are zero-cost.

Build options (see `INSTALL.md`):

```sh
zig build benchmarks -Dtracy=true                       # instrumented zones
zig build benchmarks -Dtracy=true -Dtracy-callstack=true # + sampled callstacks (needs perf access)
zig build benchmarks -Dtracy=true -Dtracy-no-exit=true   # wait for the profiler to attach before running
```

Capture a trace:

1. Start a Tracy server before/while the binary runs — either the **Tracy
   profiler GUI** (connects to `localhost:8086`) or the headless
   **`tracy-capture -o trace.tracy`** CLI (build it from the Tracy release
   matching `ext/tracy`'s pinned version; it is not vendored here).
2. Run the instrumented binary: `zig-out/bin/benchmarks` (or, with
   `-Dtracy-no-exit=true`, it blocks until the server attaches).
3. Save/open the `.tracy` trace. Zones show per-call time; `TracyCPlot` values
   (e.g. loom HWM) show as graphs.

Without a connected server the client runs with negligible overhead and simply
drops events — useful for confirming the build, not for capturing data.

**Headless capture** (no GUI, no kernel privileges) works via `tracy-capture` +
`tracy-csvexport` built from the pinned Tracy v0.12.2. See
[`PROFILING-tracy.md`](PROFILING-tracy.md) for how to build them and
[`capture-trace.sh`](capture-trace.sh) to capture a trace + per-zone summary in
one step:

```sh
TRACY_CAPTURE=./tools/tracy-capture TRACY_CSVEXPORT=./tools/tracy-csvexport \
  doc/benchmarks/capture-trace.sh trace.tracy /path/to/zig
```

> Note: Tracy's *sampling* profiler still needs `perf` access
> (`kernel.perf_event_paranoid`), but the *instrumented zones* used here do not.

### perf (Linux sampling; needs relaxed paranoia)

If you control the host:

```sh
sudo sysctl kernel.perf_event_paranoid=1     # allow user-space sampling
zig build benchmarks -Doptimize=ReleaseFast
perf stat -d -d zig-out/bin/benchmarks                       # hardware counters
perf record -g --call-graph=fp zig-out/bin/benchmarks        # frame-pointer call graph
perf report --stdio | head -60                               # hotspots
```

The release build keeps frame pointers (`-fno-omit-frame-pointer`) and `-g3`, so
`perf` resolves symbols and call graphs well. **`gprof`/`-pg` does not work**:
the build is musl-static and musl has no `mcount`.

## Conventions for comparing after changes

- Always compare at the **same commit-of-suite, same build flags, same host**.
- Re-run `run-benchmarks.sh` ≥10×; report min and median (min is the least
  noisy estimate of true cost; median guards against outliers).
- A change is "real" only if the delta exceeds the recorded `stdev` for that
  benchmark — see the per-benchmark stdev column in `baseline-2026-06-24.md`.
- Keep the determinism gate in mind: any interpreter/jet/jam change must still
  produce bit-identical Nock results (`PERFORMANCE_REPORT.md` §1.3).
