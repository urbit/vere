#!/bin/bash
# test-yolo-replay.sh: replay-from-event-1 tests against committed pier fixtures,
# with NO disk migration whatsoever.
#
# For each eligible fixture the script:
#   1. Unpacks the pier into a temporary directory.
#   2. Verifies the on-disk layout is replayable in place (flat or single 0i0
#      epoch); hard-fails on multi-epoch or non-0i0 layouts.
#   3. Deletes .urb/chk/* so replay starts from event 1, not from the existing
#      snapshot.
#   4. Runs `vere play -f -y --no-migrate <pier>` to replay every event.
#   5. Boots the pier with --lite-boot --daemon (no injected events).
#   6. Queries (mug .(now 0, eny 0)) via lensd and compares against the
#      committed .mug golden file.
#   7. Greps captured stderr for migration-related strings; any hit fails.
#
# Required environment variable:
#   VERE_BINARY     vere binary path, relative to GITHUB_WORKSPACE or PWD
#                   (caller runs this script once per bitness)
#
# Eligible fixtures are listed explicitly below; adding a new fixture without
# auditing its layout is a deliberate two-step operation.

set -xeuo pipefail

workspace=${GITHUB_WORKSPACE:-$(pwd)}
vere="$workspace/$VERE_BINARY"
fixtures_dir="$(cd "$(dirname "$0")" && pwd)"

# Explicit allowlist.
#   - zod-v3.3 has epochs 0i0 + 0i101; replay-from-event-1 is not well-defined
#     across multiple epochs.
#   - zod-v4.2 has a non-replayable boot sequence: events 1-5 bail during
#     u3v_boot (ride compiles then aborts) under any vere — including the
#     v4.2 binary that originally wrote them. Likely an OTA/imported-state
#     artifact rather than a clean cold-boot log.
ELIGIBLE=(zod-v1.21 zod-v2.12)

# ── Helpers ───────────────────────────────────────────────────────────────────

lensd() {
  local port="$1" cmd="$2"
  curl -s \
    --data "{\"source\":{\"dojo\":\"$cmd\"},\"sink\":{\"stdout\":null}}" \
    "http://localhost:$port" | xargs printf %s | sed 's/\\n/\n/g'
}

lensa() {
  local port="$1" app="$2" cmd="$3"
  curl -s \
    --data "{\"source\":{\"dojo\":\"$cmd\"},\"sink\":{\"app\":\"$app\"}}" \
    "http://localhost:$port" | xargs printf %s | sed 's/\\n/\n/g'
}

wait_for_port() {
  local pier_d="$1"
  local i
  for i in $(seq 1 60); do
    if grep -q loopback "$pier_d/.http.ports" 2>/dev/null; then
      return 0
    fi
    sleep 2
  done
  echo "timed out waiting for HTTP port in $pier_d" >&2
  return 1
}

wait_for_shutdown() {
  local pier_d="$1"
  local i
  for i in $(seq 1 60); do
    if [ ! -f "$pier_d/.vere.lock" ]; then
      return 0
    fi
    sleep 2
  done
  echo "timed out waiting for $pier_d to shut down" >&2
  return 1
}

# Hard-fail if the pier layout cannot support replay-from-event-1 without
# migration. Accepts flat-layout (no 0i* dirs) or a single 0i0 epoch.
check_layout() {
  local pier_d="$1" name="$2"
  local epochs
  epochs=$(find "$pier_d/.urb/log" -maxdepth 1 -mindepth 1 -type d -name '0i*' \
             -exec basename {} \; | sort)
  local count
  count=$(printf '%s\n' "$epochs" | sed '/^$/d' | wc -l | tr -d ' ')

  if [ "$count" -eq 0 ]; then
    return 0  # flat
  elif [ "$count" -eq 1 ] && [ "$epochs" = "0i0" ]; then
    return 0  # single 0i0
  fi

  echo "FAILED: $name: ineligible layout (epochs found: $(echo $epochs))" >&2
  return 1
}

# ── Run each eligible fixture ─────────────────────────────────────────────────

failures=0

for name in "${ELIGIBLE[@]}"; do
  fixture="$fixtures_dir/$name.tar.gz"
  mug_file="$fixtures_dir/$name.mug"

  if [ ! -f "$fixture" ]; then
    echo "FAILED: $name: fixture archive missing at $fixture" >&2
    ((failures++))
    continue
  fi

  if [ ! -f "$mug_file" ]; then
    echo "FAILED: $name: golden .mug file missing at $mug_file" >&2
    ((failures++))
    continue
  fi

  tmpdir="$(mktemp -d)"

  echo "=== yolo-replay test: $name ==="

  tar xfz "$fixture" -C "$tmpdir"

  pier=$(find "$tmpdir" -mindepth 1 -maxdepth 1 -type d | head -1)
  if [ -z "$pier" ]; then
    echo "FAILED: $name: could not find pier directory" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  if ! check_layout "$pier" "$name"; then
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  # Delete snapshot so replay starts from event 1. Covers both legacy
  # north.bin/south.bin and new-format image.bin.
  rm -f "$pier/.urb/chk/"*.bin
  rm -f "$pier/.urb/bhk/"*.bin

  # ── replay ──
  play_stderr="$tmpdir/vere-play-stderr.log"
  set +e
  "$vere" play -f -y --no-migrate "$pier" 2>"$play_stderr"
  play_exit=$?
  set -e

  if [ $play_exit -ne 0 ]; then
    echo "FAILED: $name: vere play exited $play_exit" >&2
    cat "$play_stderr" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  # Fail loud if any migration path ran despite --no-migrate.
  if grep -qiE "migrat|epoch roll|epoc_roll" "$play_stderr"; then
    echo "FAILED: $name: migration-related output during replay" >&2
    cat "$play_stderr" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  # ── boot + mug check ──
  # --no-migrate ensures the boot path doesn't try to migrate the flat-layout
  # log it sees on disk (replay does not rewrite the log structure).
  boot_stderr="$tmpdir/vere-boot-stderr.log"
  set +e
  "$vere" --no-migrate --lite-boot --daemon "$pier" 2>"$boot_stderr"
  boot_exit=$?
  set -e

  if [ $boot_exit -ne 0 ]; then
    echo "FAILED: $name: vere exited $boot_exit during boot" >&2
    cat "$boot_stderr" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  if ! wait_for_port "$pier"; then
    echo "FAILED: $name: ship did not come up" >&2
    cat "$boot_stderr" >&2
    if [ -f "$pier/.vere.lock" ]; then
      kill "$(< "$pier/.vere.lock")" 2>/dev/null || true
    fi
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

  if grep -qiE "migrat|epoch roll|epoc_roll" "$boot_stderr"; then
    echo "FAILED: $name: migration-related output during boot" >&2
    cat "$boot_stderr" >&2
    lensa "$port" hood '+hood/exit' || true
    wait_for_shutdown "$pier" || true
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  computed_mug=$(lensd "$port" '(mug .(now 0, eny 0))')
  expected_mug=$(< "$mug_file")
  if [ "$computed_mug" != "$expected_mug" ]; then
    echo "FAILED: $name: arvo state mug mismatch (expected $expected_mug, got $computed_mug)" >&2
    lensa "$port" hood '+hood/exit' || true
    wait_for_shutdown "$pier" || true
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi
  echo "$name: mug verified ($computed_mug)"

  lensa "$port" hood '+hood/exit'
  wait_for_shutdown "$pier"

  echo "=== $name: yolo-replay succeeded ==="
  rm -rf "$tmpdir"
done

if [ $failures -gt 0 ]; then
  echo "test-yolo-replay: $failures fixture(s) failed" >&2
  exit 1
fi

echo "test-yolo-replay: all fixtures passed"
