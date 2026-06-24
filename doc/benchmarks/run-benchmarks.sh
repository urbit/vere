#!/usr/bin/env bash
#
# run-benchmarks.sh — build the Vere microbenchmark suite in ReleaseFast and
# run it N times, emitting a min/median/mean/max/stdev table per benchmark.
#
# This is the reproducible harness behind doc/benchmarks/baseline-*.md. It does
# NOT modify the runtime; it only builds and times pkg/vere/benchmarks.c so we
# have a stable record to compare optimization work against.
#
# Usage:
#   doc/benchmarks/run-benchmarks.sh [N_RUNS] [ZIG]
#
#   N_RUNS  number of repetitions (default 10)
#   ZIG     path to the zig 0.15.2 binary (default: `zig` on PATH)
#
# Notes:
#   * The project pins zig 0.15.2. zig 0.16+ rejects build.zig.zon and will fail.
#   * The benchmark prints to stderr at millisecond granularity (gettimeofday).
#     Phases under ~1ms (e.g. "opcode 10 1k list") read as 0 and are not
#     meaningful; rely on the larger phases for tracking.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

N_RUNS="${1:-10}"
ZIG="${2:-zig}"

if ! "$ZIG" version 2>/dev/null | grep -q '^0\.15\.'; then
  echo "warning: expected zig 0.15.x, got: $("$ZIG" version 2>&1 || echo none)" >&2
  echo "         the project pins 0.15.2; newer zig will not parse build.zig.zon." >&2
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

echo "==> building benchmarks (ReleaseFast)…" >&2
"$ZIG" build benchmarks -Doptimize=ReleaseFast >/dev/null 2>&1 || {
  # the `benchmarks` step both builds and runs; re-run to surface errors
  "$ZIG" build benchmarks -Doptimize=ReleaseFast
}
BIN="zig-out/bin/benchmarks"
[ -x "$BIN" ] || { echo "error: $BIN not built" >&2; exit 1; }

echo "==> running ${N_RUNS}x…" >&2
for i in $(seq 1 "$N_RUNS"); do
  "$BIN" 2>"$OUT/run_$i.txt" >/dev/null
done

echo "==> provenance" >&2
echo "commit:  $(git rev-parse --short HEAD) $(git log -1 --format='%s')"
echo "version: $(cat pkg/vere/VERSION)"
echo "cpu:     $(lscpu 2>/dev/null | sed -n 's/^Model name: *//p' | head -1)"
echo "kernel:  $(uname -sr)"
echo "zig:     $("$ZIG" version)"
echo

python3 - "$OUT" "$N_RUNS" <<'PY'
import sys, re, glob, statistics as st
out, n = sys.argv[1], int(sys.argv[2])
data, order = {}, []
for f in sorted(glob.glob(out + "/run_*.txt")):
    for line in open(f):
        m = re.match(r'^  (.+?):\s*(\d+) ms\s*$', line.rstrip())
        if m:
            lbl, v = m.group(1), int(m.group(2))
            data.setdefault(lbl, []).append(v)
            if lbl not in order:
                order.append(lbl)
print(f"{'benchmark':<22} {'min':>5} {'med':>6} {'mean':>7} {'max':>5} {'stdev':>6}  (n={n}, ms)")
print("-" * 66)
for lbl in order:
    v = data[lbl]
    print(f"{lbl:<22} {min(v):>5} {st.median(v):>6} {st.mean(v):>7.1f} {max(v):>5} {st.pstdev(v):>6.1f}")
PY
