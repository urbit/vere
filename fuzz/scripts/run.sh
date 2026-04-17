#!/usr/bin/env bash
# Launch an AFL++ campaign against a single harness.
#
# Usage: ./fuzz/scripts/run.sh <harness> [n_cores]
#
# Example:
#   ./fuzz/scripts/run.sh fuzz_ur_cue 8

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FUZZ_DIR="$ROOT/fuzz"
OUT_BASE="$FUZZ_DIR/out/sessions"

HARNESS="${1:?usage: run.sh <harness> [n_cores]}"
CORES="${2:-1}"

BIN="$FUZZ_DIR/out/${HARNESS}.afl"
CMPLOG="$FUZZ_DIR/out/${HARNESS}.cmplog"
CORPUS="$FUZZ_DIR/corpus/${HARNESS}"
DICT=""
for cand in "$FUZZ_DIR/dicts/${HARNESS}.dict" "$FUZZ_DIR/dicts/cue.dict"; do
  if [[ -f "$cand" ]]; then DICT="$cand"; break; fi
done

if [[ ! -x "$BIN" ]]; then
  echo "error: $BIN not built. Run ./fuzz/build.sh $HARNESS" >&2
  exit 1
fi
if [[ ! -d "$CORPUS" || -z "$(ls -A "$CORPUS" 2>/dev/null)" ]]; then
  echo "error: corpus $CORPUS missing or empty" >&2
  exit 1
fi

SESSION_DIR="$OUT_BASE/${HARNESS}-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$SESSION_DIR"

# Host sanity checks
if [[ "$(cat /proc/sys/kernel/core_pattern 2>/dev/null)" != "core" ]]; then
  echo "warning: /proc/sys/kernel/core_pattern != core"
  echo "  run: sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'"
  echo "  (continuing — afl-fuzz will complain if it's a problem)"
fi

common_args=(
  -i "$CORPUS"
  -o "$SESSION_DIR"
  -t 1000
  -m none
)
[[ -n "$DICT" ]] && common_args+=(-x "$DICT")

export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1
export AFL_SKIP_CPUFREQ=1

# Vere's loom maxes out at 16 GiB; any allocation above that is a real
# bug, not a harness artifact. ASan default is 10 TB which lets DoS
# inputs slip by as "too-big-to-service but not-technically-illegal";
# cap to 16 GiB so the fuzzer flags excess-size inputs as crashes
# directly. Lazy zero pages mean the process RSS won't actually track
# virtual size for calloc.
#
# symbolize=0 is required at fuzz time — AFL++ refuses to launch
# otherwise because symbolization is slow. Triage scripts (which run
# the harness standalone against saved crashes) should set
# symbolize=1 themselves.
export ASAN_OPTIONS="abort_on_error=1:symbolize=0:detect_leaks=0:max_allocation_size_mb=16384:allocator_may_return_null=0"
export UBSAN_OPTIONS="abort_on_error=1:symbolize=0"

# Harness-scoped session tag so multiple harnesses can run in parallel
# without colliding on screen session names or AFL fuzzer IDs.
TAG="${HARNESS#fuzz_}"

if [[ "$CORES" -eq 1 ]]; then
  echo "launching single instance: $HARNESS"
  exec afl-fuzz "${common_args[@]}" -c "$CMPLOG" -- "$BIN"
fi

echo "launching $CORES instances of $HARNESS into $SESSION_DIR"
echo "attach to main with: screen -r afl-${TAG}-main"

# Main instance with CMPLOG
screen -dmS "afl-${TAG}-main" \
  afl-fuzz "${common_args[@]}" -M "${TAG}-main" -c "$CMPLOG" -- "$BIN"

for i in $(seq 1 $((CORES - 1))); do
  screen -dmS "afl-${TAG}-s${i}" \
    afl-fuzz "${common_args[@]}" -S "${TAG}-s${i}" -- "$BIN"
done

echo
echo "running sessions for $HARNESS:"
screen -ls | grep "afl-${TAG}-"
echo
echo "output: $SESSION_DIR"
echo "stop all: ./fuzz/scripts/stop.sh"
