#!/usr/bin/env bash
# One-line status per running harness. Reads each session's
# fuzzer_stats and prints a compact dashboard.
#
# Usage:
#   ./fuzz/scripts/status.sh           # one shot
#   ./fuzz/scripts/status.sh --watch   # refresh every 5s
#   ./fuzz/scripts/status.sh --watch 10  # refresh every N seconds

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SESSIONS="$ROOT/fuzz/out/sessions"

WATCH=0
INTERVAL=5
if [[ ${1:-} == "--watch" ]]; then
  WATCH=1
  INTERVAL=${2:-5}
fi

read_stat() {
  local f=$1 key=$2
  awk -F: -v k="$key" '$1 ~ k {gsub(/ /,"",$2); print $2; exit}' "$f"
}

print_status() {
  printf "%-22s  %10s  %8s  %7s  %8s  %5s  %s\n" \
    "harness" "execs" "exec/s" "edges" "cvg%" "crsh" "runtime"
  printf "%-22s  %10s  %8s  %7s  %8s  %5s  %s\n" \
    "----------------------" "----------" "--------" "-------" "--------" "-----" "-------"

  local total_execs=0
  local total_crashes=0
  local total_running=0

  # Find the most recent session per harness.
  declare -A latest
  for d in "$SESSIONS"/*/; do
    [[ -d "$d" ]] || continue
    name=$(basename "$d")
    # name format: fuzz_X_Y-YYYYMMDD-HHMMSS
    h="${name%-*-*}"
    if [[ -z "${latest[$h]:-}" ]] || [[ "$d" > "${latest[$h]}" ]]; then
      latest[$h]="$d"
    fi
  done

  for h in $(printf '%s\n' "${!latest[@]}" | sort); do
    d="${latest[$h]}"
    main_d="$d/${h#fuzz_}-main"
    [[ -d "$main_d" ]] || main_d=$(find "$d" -maxdepth 1 -mindepth 1 -type d | head -1)
    stats="$main_d/fuzzer_stats"
    if [[ ! -f "$stats" ]]; then
      printf "%-22s  %10s\n" "${h#fuzz_}" "(no stats)"
      continue
    fi

    execs=$(read_stat "$stats" execs_done)
    eps=$(read_stat "$stats" execs_per_sec)
    edges=$(read_stat "$stats" edges_found)
    cvg=$(read_stat "$stats" bitmap_cvg)
    crsh=$(read_stat "$stats" saved_crashes)
    rt=$(read_stat "$stats" run_time)

    # is the fuzzer process still alive?
    pid=$(read_stat "$stats" fuzzer_pid)
    alive=" "
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      alive="*"
      total_running=$((total_running + 1))
    fi

    rt_fmt=$(printf "%02d:%02d:%02d" $((rt/3600)) $(((rt%3600)/60)) $((rt%60)))

    printf "%-22s  %10s  %8s  %7s  %8s  %5s  %s%s\n" \
      "${h#fuzz_}" "$execs" "$eps" "$edges" "$cvg" "$crsh" "$rt_fmt" "$alive"

    total_execs=$((total_execs + execs))
    total_crashes=$((total_crashes + crsh))
  done

  printf "%-22s  %10s  %8s  %7s  %8s  %5s\n" \
    "TOTAL" "$total_execs" "" "" "" "$total_crashes"
  echo "$total_running fuzzer(s) running   (* = alive)"
}

if [[ $WATCH -eq 1 ]]; then
  while true; do
    clear
    date '+%Y-%m-%d %H:%M:%S'
    echo
    print_status
    sleep "$INTERVAL"
  done
else
  print_status
fi
