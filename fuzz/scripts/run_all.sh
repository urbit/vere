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
  # Phase 1 ur + noun cue
  fuzz_ur_cue
  fuzz_ur_cue_test
  fuzz_ur_jam_cue_diff
  fuzz_u3_cue_bytes
  fuzz_u3_cue_xeno
  # Phase 1 IO / disk / jets
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
  # Phase 2 ...
  fuzz_mesa_bitset
  fuzz_mesa_page_flow
  fuzz_u3r_sing
  fuzz_fore_inject
  fuzz_http_request
  fuzz_lick_ipc
  # Phase 3 data-entry sub-parsers
  # fuzz_ames_head — DROPPED: target is pure bit-shifts with zero
  # branches; 3 "paths" were all scaffolding. Per audit.
  fuzz_ames_prel
  fuzz_lane_decode
  fuzz_fine_wail
  fuzz_fine_meow
  fuzz_stun_xor
  fuzz_cttp_head
  fuzz_cttp_body
  fuzz_http_range
  fuzz_http_cookie
  fuzz_tls_pem
  fuzz_ed_veri
  fuzz_secp_reco
  fuzz_secp_schnorr
  fuzz_argon2
  fuzz_blake2b
  fuzz_aes_siv
  fuzz_jet_trip
  fuzz_jet_leer
  fuzz_jet_lore
  fuzz_jet_cut
  fuzz_jet_rip_rep
  fuzz_jet_lsh
  fuzz_jet_hew_sew
  fuzz_nock_mink
  fuzz_parse_combi
  fuzz_bytestream
  # Phase 4 serial.c aura parsers / etchers
  fuzz_serial_sift_ud
  fuzz_serial_rtrip
  fuzz_serial_etch_all
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
  # afl-fuzz rejects -M sync_id > 24 chars with a silent abort inside
  # the detached screen session. Catch it here so the failure is loud.
  if (( ${#tag} + 5 > 24 )); then
    echo "error: harness name '${h}' produces -M tag '${tag}-main' (${#tag}+5 chars) that exceeds afl-fuzz's 24-char limit. Rename the harness." >&2
    exit 2
  fi
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
