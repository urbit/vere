#!/usr/bin/env bash
# Launch every built harness on its own core in the background.
# One AFL++ instance per harness, persistent + CMPLOG, named screen
# session. Stop with ./fuzz/scripts/stop.sh.
#
# Usage:
#   ./fuzz/scripts/run_all.sh           # launch all harnesses
#   ./fuzz/scripts/run_all.sh H1 H4 H7  # launch only these

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FUZZ_DIR="$ROOT/fuzz"
OUT_BASE="$FUZZ_DIR/out/sessions"
mkdir -p "$OUT_BASE"

# All harnesses currently in the build, in priority order.
ALL_HARNESSES=(
  fuzz_ur_cue
  fuzz_ur_cue_test
  fuzz_ur_jam_cue_diff
  fuzz_u3_cue_bytes
  fuzz_u3_cue_xeno
  fuzz_newt_decode
  fuzz_mesa_sift_pact
  fuzz_ames_sift_packet
  fuzz_stun_response
  fuzz_lss_ingest
  fuzz_disk_sift
  fuzz_ce_patch_control
  fuzz_past_v4_load
  fuzz_json_de
  fuzz_conn_poke
  fuzz_scot_slaw
  fuzz_zlib_de
  fuzz_base_decode
)

if [[ $# -gt 0 ]]; then
  HARNESSES=("$@")
else
  HARNESSES=("${ALL_HARNESSES[@]}")
fi

# Sanity checks
if [[ "$(cat /proc/sys/kernel/core_pattern 2>/dev/null)" != "core" ]]; then
  echo "warning: /proc/sys/kernel/core_pattern != core"
  echo "  fix:   sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'"
fi

export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1
export AFL_SKIP_CPUFREQ=1
export AFL_NO_UI=1
export AFL_AUTORESUME=1
# Allow more harnesses than physical cores. AFL++ refuses to bind to
# an already-claimed core by default, so without this we'd silently
# lose any harness past the 16th on this 16-core box.
export AFL_NO_AFFINITY=1
export ASAN_OPTIONS="abort_on_error=1:symbolize=0:detect_leaks=0:max_allocation_size_mb=16384:allocator_may_return_null=0"
export UBSAN_OPTIONS="abort_on_error=1:symbolize=0"

launched=0
skipped=0
ts=$(date +%Y%m%d-%H%M%S)

for h in "${HARNESSES[@]}"; do
  bin="$FUZZ_DIR/out/${h}.afl"
  cmplog="$FUZZ_DIR/out/${h}.cmplog"
  corpus="$FUZZ_DIR/corpus/${h}"

  if [[ ! -x "$bin" ]]; then
    echo "skip: $h (not built; run ./fuzz/build.sh $h)"
    skipped=$((skipped + 1))
    continue
  fi
  if [[ ! -d "$corpus" ]] || [[ -z "$(ls -A "$corpus" 2>/dev/null)" ]]; then
    echo "skip: $h (no corpus at $corpus)"
    skipped=$((skipped + 1))
    continue
  fi

  tag="${h#fuzz_}"
  session_dir="$OUT_BASE/${h}-${ts}"
  mkdir -p "$session_dir"

  # Pick a dictionary if one matches the harness or its category.
  dict=""
  for cand in "$FUZZ_DIR/dicts/${h#fuzz_}.dict" \
              "$FUZZ_DIR/dicts/${h}.dict" \
              "$FUZZ_DIR/dicts/cue.dict"; do
    if [[ -f "$cand" ]]; then dict="$cand"; break; fi
  done

  args=(
    -i "$corpus"
    -o "$session_dir"
    -t 1000
    -m none
    -M "${tag}-main"
    -c "$cmplog"
  )
  [[ -n "$dict" ]] && args+=(-x "$dict")

  echo "launch $h → screen afl-${tag}-main"
  screen -dmS "afl-${tag}-main" \
    afl-fuzz "${args[@]}" -- "$bin"
  launched=$((launched + 1))
done

echo
echo "launched $launched, skipped $skipped"
echo "monitor with: ./fuzz/scripts/status.sh"
echo "stop with:    ./fuzz/scripts/stop.sh"
