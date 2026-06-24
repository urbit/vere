#!/usr/bin/env bash
#
# pier-bench.sh — pier/eval-level benchmarks that the standalone
# pkg/vere/benchmarks.c harness cannot reach:
#
#   Tier 1 (always; no network): WARM jet dispatch via `urbit eval`. The
#     embedded ivory pill loads the jetted kernel, so dec/add/etc. dispatch to
#     their C jets — unlike the cold/unregistered slam in benchmarks.c.
#
#   Tier 2 (needs a pill): BOOT, SNAPSHOT, and REPLAY timing for a fake ship.
#     Boots `-F bus` from a brass pill + arvo, times the boot (which writes the
#     first snapshot), reports snapshot size, then times a from-events replay.
#
# Usage:
#   doc/benchmarks/pier-bench.sh [N_RUNS]
#
# Env:
#   URBIT   path to the urbit binary (default: zig-out/<triple>/urbit)
#   PILL    path to a brass pill        (Tier 2; else skipped)
#   ARVO    path to an arvo source dir  (Tier 2; required with a brass pill)
#   WORK    scratch dir for the pier    (default: mktemp)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

N_RUNS="${1:-5}"

URBIT="${URBIT:-}"
if [ -z "$URBIT" ]; then
  URBIT="$(find zig-out -type f -name urbit -executable 2>/dev/null | head -1 || true)"
fi
[ -n "$URBIT" ] && [ -x "$URBIT" ] || { echo "error: urbit binary not found; build with 'zig build' or set URBIT=" >&2; exit 1; }
echo "urbit: $URBIT"

# minimum wall-clock over N runs of `urbit eval <expr>`
_eval_min_ms() {
  local expr="$1" lo=1e9 i s e d
  for i in $(seq 1 "$N_RUNS"); do
    s=$(date +%s.%N)
    printf '%s\n' "$expr" | "$URBIT" eval >/dev/null 2>&1 || true
    e=$(date +%s.%N)
    d=$(awk "BEGIN{print $e-$s}")
    lo=$(awk "BEGIN{print ($d<$lo)?$d:$lo}")
  done
  awk "BEGIN{printf \"%.0f\", $lo*1000}"
}

echo
echo "== Tier 1: warm jet dispatch (urbit eval) =="
START=$(_eval_min_ms '(add 2 3)')
DEC=$(_eval_min_ms   '=+(n=1.000.000 |-(?:(=(0 n) n $(n (dec n)))))')
ACK=$(_eval_min_ms   '=/(ack |=([m=@ n=@] ?:(=(0 m) +(n) ?:(=(0 n) $(m (dec m), n 1) $(m (dec m), n $(n (dec n)))))) (ack 3 7))')
printf "  %-26s %6s ms   (ivory load + compile; subtract as the fixed cost)\n" "eval startup (add 2 3)" "$START"
printf "  %-26s %6s ms   (~%s ms compute -> jetted dec x1,000,000)\n" "jetted dec 1M"   "$DEC" "$((DEC-START))"
printf "  %-26s %6s ms   (~%s ms compute -> jetted ack(3,7), ~690k calls)\n" "jetted ack(3,7)" "$ACK" "$((ACK-START))"

# ---- Tier 2: boot / snapshot / replay ----
PILL="${PILL:-}"
ARVO="${ARVO:-}"
if [ -z "$PILL" ] || [ ! -f "$PILL" ]; then
  echo
  echo "== Tier 2: boot/replay/snapshot — SKIPPED (set PILL=<brass.pill> ARVO=<arvo-dir>) =="
  echo "   e.g. fetch the maintainer-tested pair (see boot-fake-ship.sh):"
  echo "     REV=88c6173048d61ebd86455f0c1a8ce8f8099cbe01"
  echo "     curl -sLJ -o brass.pill https://github.com/urbit/urbit/raw/\$REV/bin/brass.pill"
  echo "     curl -sLJ https://github.com/urbit/urbit/archive/\$REV.tar.gz | tar xz"
  exit 0
fi

WORK="${WORK:-$(mktemp -d)}"
PIER="$WORK/pier"
rm -rf "$PIER"
echo
echo "== Tier 2: boot / snapshot / replay  (pier: $PIER) =="

ARVO_ARG=()
[ -n "$ARVO" ] && ARVO_ARG=(--arvo "$ARVO")

s=$(date +%s.%N)
"$URBIT" -t --lite-boot --fake bus --bootstrap "$PILL" "${ARVO_ARG[@]}" --pier "$PIER" -x >/dev/null 2>&1 || true
e=$(date +%s.%N)
printf "  %-26s %6.1f s\n" "boot (-F bus, -x)" "$(awk "BEGIN{print $e-$s}")"

if [ -d "$PIER/.urb/chk" ]; then
  printf "  %-26s %6s\n" "snapshot size (.urb/chk)" "$(du -sh "$PIER/.urb/chk" | cut -f1)"
fi
if [ -d "$PIER/.urb/log" ]; then
  printf "  %-26s %6s\n" "event log size (.urb/log)" "$(du -sh "$PIER/.urb/log" | cut -f1)"
fi

# Replay from events: -D recomputes from the log, -x exits when caught up.
s=$(date +%s.%N)
"$URBIT" -t --replay -x "$PIER" >/dev/null 2>&1 || true
e=$(date +%s.%N)
printf "  %-26s %6.1f s\n" "replay (--replay, -x)" "$(awk "BEGIN{print $e-$s}")"

echo "  (pier left at $PIER; rm -rf to clean up)"
