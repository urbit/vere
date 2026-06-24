#!/usr/bin/env bash
#
# capture-trace.sh — build the Tracy-instrumented benchmark and capture a
# .tracy trace headlessly (no GUI), then optionally dump a per-zone summary.
#
# Requires `tracy-capture` (and optionally `tracy-csvexport`) built from the
# Tracy version pinned in ext/tracy/build.zig.zon (v0.12.2). See
# PROFILING-tracy.md for how to build them.
#
# Usage:
#   doc/benchmarks/capture-trace.sh [OUT.tracy] [ZIG] [TRACY_CAPTURE] [TRACY_CSVEXPORT]
#
# Env overrides:
#   ZIG               path to zig 0.15.2          (default: `zig`)
#   TRACY_CAPTURE     path to tracy-capture       (default: search PATH, ./tools)
#   TRACY_CSVEXPORT   path to tracy-csvexport     (default: search PATH, ./tools; optional)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

OUT="${1:-trace.tracy}"
ZIG="${2:-${ZIG:-zig}}"
CAP="${3:-${TRACY_CAPTURE:-}}"
CSV="${4:-${TRACY_CSVEXPORT:-}}"

# locate tracy-capture if not given
if [ -z "$CAP" ]; then
  for c in tracy-capture "$REPO_ROOT/../tools/tracy-capture" "$REPO_ROOT/tools/tracy-capture"; do
    if command -v "$c" >/dev/null 2>&1; then CAP="$(command -v "$c")"; break; fi
    [ -x "$c" ] && { CAP="$c"; break; }
  done
fi
[ -n "$CAP" ] && [ -x "$CAP" ] || { echo "error: tracy-capture not found; see PROFILING-tracy.md" >&2; exit 1; }

# locate tracy-csvexport (optional)
if [ -z "$CSV" ]; then
  for c in tracy-csvexport "$REPO_ROOT/../tools/tracy-csvexport" "$REPO_ROOT/tools/tracy-csvexport"; do
    if command -v "$c" >/dev/null 2>&1; then CSV="$(command -v "$c")"; break; fi
    [ -x "$c" ] && { CSV="$c"; break; }
  done
fi

echo "==> building instrumented benchmark (-Dtracy=true)…" >&2
"$ZIG" build benchmarks -Doptimize=ReleaseFast -Dtracy=true >/dev/null 2>&1 || true
BIN="zig-out/bin/benchmarks"
[ -x "$BIN" ] || { echo "error: $BIN not built" >&2; exit 1; }

# Capture-first: start tracy-capture so it is already waiting, then run the
# benchmark. The Tracy client (the benchmark) is the server on :8086; capture
# connects to it the instant it starts and drains through the ~2s run.
echo "==> starting tracy-capture -> $OUT" >&2
rm -f "$OUT"
"$CAP" -o "$OUT" -f &
CAP_PID=$!
sleep 1

echo "==> running benchmark…" >&2
"$BIN" >/dev/null 2>&1 || true

wait "$CAP_PID" || true
echo "==> wrote $OUT ($(du -h "$OUT" 2>/dev/null | cut -f1))" >&2

# optional per-zone summary
if [ -n "${CSV:-}" ] && [ -x "$CSV" ]; then
  echo "==> per-zone summary (exec time incl. children):" >&2
  "$CSV" -u "$OUT" 2>/dev/null | python3 - <<'PY'
import sys,csv,statistics as st
rows=list(csv.DictReader(sys.stdin))
agg={}
for r in rows:
    try: t=int(r['exec_time_ns'])
    except: continue
    agg.setdefault(r['name'],[]).append(t)
print(f"{'zone':<18}{'count':>8}{'total_ms':>10}{'mean_ns':>10}{'med_ns':>9}")
print('-'*55)
for name,v in sorted(agg.items(), key=lambda kv:-sum(kv[1])):
    print(f"{name:<18}{len(v):>8}{sum(v)/1e6:>10.1f}{st.mean(v):>10.0f}{st.median(v):>9.0f}")
PY
fi

echo "Open $OUT in the Tracy profiler GUI for the full timeline." >&2
