#!/usr/bin/env bash
# Deterministically replay every regression corpus entry through its
# corresponding harness. Intended for PR-smoke CI: fast, offline, no
# mutation, no fuzzing — just proves that every previously-triaged
# bug remains fixed.
#
# Usage:
#   ./fuzz/scripts/replay.sh             # replay every harness's regression set
#   ./fuzz/scripts/replay.sh fuzz_ur_cue # replay one harness
#
# Exits 0 if every input exits 0 or is rejected (non-crash non-zero).
# Exits 1 if any input triggers a crash / abort / ASan report.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FUZZ_DIR="$ROOT/fuzz"
OUT_DIR="$FUZZ_DIR/out"
REGRESSION_DIR="$FUZZ_DIR/regression"

# Symbolised ASan so crash reports are readable if any fire.
export ASAN_OPTIONS="abort_on_error=1:symbolize=1:detect_leaks=0:max_allocation_size_mb=16384:allocator_may_return_null=0"
export UBSAN_OPTIONS="abort_on_error=1:symbolize=1:print_stacktrace=1"

crashing=0
replayed=0
checked_harnesses=0

replay_one() {
  local harness="$1"
  local bin="$OUT_DIR/${harness}.afl"
  local corpus="$REGRESSION_DIR/${harness}"

  if [[ ! -x "$bin" ]]; then
    echo "skip: $harness (not built — run ./fuzz/build.sh $harness)"
    return 0
  fi

  if [[ ! -d "$corpus" ]] || [[ -z "$(ls -A "$corpus" 2>/dev/null)" ]]; then
    echo "skip: $harness (no regression corpus)"
    return 0
  fi

  checked_harnesses=$((checked_harnesses + 1))

  local harness_crashes=0
  for f in "$corpus"/*; do
    [[ -f "$f" ]] || continue
    replayed=$((replayed + 1))

    local out
    out=$(timeout 10 "$bin" < "$f" 2>&1)
    local code=$?

    # AFL++ / ASan report crashes via:
    #   exit code 134 (SIGABRT via abort_on_error)
    #   exit code 139 (SIGSEGV)
    #   exit code 137 (SIGKILL — treat as timeout = failure too)
    #   timeout (exit code 124)
    if [[ $code -eq 134 || $code -eq 139 || $code -eq 137 || $code -eq 124 ]]; then
      crashing=$((crashing + 1))
      harness_crashes=$((harness_crashes + 1))
      echo
      echo "CRASH: $harness $(basename "$f") (exit $code)"
      echo "$out" | sed 's/^/    /'
    fi
  done

  if [[ $harness_crashes -eq 0 ]]; then
    local count
    count=$(ls -1 "$corpus" | wc -l)
    echo "ok:   $harness ($count reproducers)"
  fi
}

if [[ $# -gt 0 ]]; then
  for h in "$@"; do
    replay_one "$h"
  done
else
  for d in "$REGRESSION_DIR"/*/; do
    [[ -d "$d" ]] || continue
    replay_one "$(basename "$d")"
  done
fi

echo
echo "replayed $replayed reproducer(s) across $checked_harnesses harness(es)"
if [[ $crashing -gt 0 ]]; then
  echo "FAIL: $crashing crash(es) detected"
  exit 1
fi
echo "PASS"
exit 0
