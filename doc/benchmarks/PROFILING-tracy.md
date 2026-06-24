# Building & using `tracy-capture` (headless Tracy profiling)

Tracy's GUI profiler is the usual way to view a trace, but on a headless box
you capture traces with **`tracy-capture`** (a CLI server that records a running
Tracy client to a `.tracy` file) and inspect them with **`tracy-csvexport`** (or
later in the GUI). Tracy's *instrumented zones* — what we added in
`pkg/noun/tracy.h` and the entry points — need **no kernel privileges**, so this
works even where `perf` is blocked by `perf_event_paranoid`.

## Version must match the pinned client

The Tracy client compiled into the runtime is pinned in
`ext/tracy/build.zig.zon` → **v0.12.2**. `tracy-capture` checks the protocol
version, so build the tools from the **same** tag or they refuse to connect.

## Build the tools (no sudo required)

The capture/csvexport tools need CMake ≥3.16 and a C++20 compiler. They
**auto-download** their real dependencies (capstone, zstd, ppqsort) via CPM, so
no dev packages are needed — the only catch is that upstream's `vendor.cmake`
also configures GUI deps (glfw/freetype/imgui/nfd); `-DNO_FILESELECTOR=ON` drops
the one that fails without `libdbus-1-dev`, and the capture/csvexport targets
never link the rest.

```sh
# 0) get the Tracy v0.12.2 source. Easiest: `zig build -Dtracy=true` once, which
#    caches it under ~/.cache/zig/p/<hash>/ ; or download:
curl -fsSL https://github.com/wolfpld/tracy/archive/v0.12.2.tar.gz | tar xz
cd tracy-0.12.2     # (or: cp -r <zig-cache>/<hash> tracy-src && cd tracy-src && chmod -R u+w .)

# 1) need a modern cmake? the system one may be too old. grab a static build:
#    curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.tar.gz | tar xz
#    CMAKE=$PWD/cmake-3.30.5-linux-x86_64/bin/cmake     (else: CMAKE=cmake)

# 2) build tracy-capture
$CMAKE -S capture   -B capture/build   -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
$CMAKE --build capture/build   --target tracy-capture   -j"$(nproc)"

# 3) build tracy-csvexport (optional; for headless per-zone stats)
$CMAKE -S csvexport -B csvexport/build -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
$CMAKE --build csvexport/build --target tracy-csvexport -j"$(nproc)"

# binaries: capture/build/tracy-capture , csvexport/build/tracy-csvexport
# they are self-contained apart from libstdc++/libc; copy them onto PATH or to ./tools/
```

### Faster, with sudo

If you have root, the dev packages let you skip the CPM downloads and the
`NO_FILESELECTOR` workaround:

```sh
sudo apt-get install -y cmake g++ pkg-config libcapstone-dev libzstd-dev libdbus-1-dev
# then the same cmake commands without -DNO_FILESELECTOR (capstone/zstd come from the system)
```

(Ubuntu's packaged `tracy`/`tracy-capture`, if any, will be too old to match
v0.12.2 — build from source.)

## Capture a trace

The Tracy **client is the server**: the instrumented benchmark listens on
:8086, and `tracy-capture` connects *to it*. So start capture first, then run
the benchmark — it connects the instant the client starts and drains through the
~2 s run. The helper does this for you:

```sh
# with the tools on PATH or in ./tools/
TRACY_CAPTURE=./tools/tracy-capture TRACY_CSVEXPORT=./tools/tracy-csvexport \
  doc/benchmarks/capture-trace.sh trace.tracy /path/to/zig
```

Manually:

```sh
zig build benchmarks -Doptimize=ReleaseFast -Dtracy=true       # build instrumented binary
./tools/tracy-capture -o trace.tracy -f &                       # start capture (waits)
sleep 1
zig-out/bin/benchmarks                                          # run; capture drains it
wait
```

`-Dtracy-no-exit=true` (make the client block at exit for a profiler) is *not*
required with the capture-first ordering, and only takes effect if the Tracy
client library itself is compiled with `TRACY_NO_EXIT` — capture-first is the
reliable path.

## Inspect headlessly

```sh
./tools/tracy-csvexport -u trace.tracy > zones.csv     # one row per zone instance
# aggregate (see capture-trace.sh for the python one-liner), or open in the GUI.
```

Example output from the current instrumentation (Ryzen 7800X3D, ReleaseFast):

```
zone                count  total_ms   mean_ns   med_ns
-------------------------------------------------------
bench:cue               1    1331.1        —         —      (outer group span)
bench:jam               1     315.3        —         —
bench:edit              1      99.9        —         —
bench:cue_soft          1      72.6        —         —
u3s_cue             40000     122.0      3049      2755     legacy noun-arith cue
u3s_cue_xeno        20000      24.3      1213       802     bitstream cue
u3s_jam_fib         10002       9.3       930       852
u3s_jam_xeno        10001       7.7       773       722
```

Note `u3s_cue` averages ~2.5× the per-call cost of `u3s_cue_xeno` — consistent
with the report's recommendation to retire the legacy path (finding S7).
`exec_time` includes children; use `tracy-csvexport -e` for self time once you
add nested zones, or capture with statistics enabled
(`-DNO_STATISTICS=OFF` when building the tools) and use the GUI's statistics view.
