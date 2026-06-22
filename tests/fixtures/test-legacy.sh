#!/bin/bash
# test-legacy.sh: run v1–v4 loom migration tests against committed pier fixtures.
#
# For each *.tar.gz fixture in this directory the script:
#   1. Unpacks the pier into a temporary directory.
#   2. Boots current 32-bit vere on it (triggers automatic loom migration).
#   3. Verifies the ship responds to a basic dojo command.
#   4. Sends +hood/exit and waits for a clean shutdown.
#
# Required environment variables:
#   VERE32_BINARY   32-bit vere binary path, relative to GITHUB_WORKSPACE or PWD

set -xeuo pipefail

workspace=${GITHUB_WORKSPACE:-$(pwd)}
vere32="$workspace/$VERE32_BINARY"
fixtures_dir="$(cd "$(dirname "$0")" && pwd)"

# ── Check for fixtures ────────────────────────────────────────────────────────

shopt -s nullglob
fixtures=("$fixtures_dir"/*.tar.gz)
shopt -u nullglob

if [ "${#fixtures[@]}" -eq 0 ]; then
  echo "test-legacy: no fixture archives found in $fixtures_dir"
  echo "test-legacy: see tests/fixtures/README.md for instructions on creating fixtures"
  echo "test-legacy: skipping"
  exit 0
fi

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

# ── Run each fixture ──────────────────────────────────────────────────────────

failures=0

for fixture in "${fixtures[@]}"; do
  name="$(basename "$fixture" .tar.gz)"
  tmpdir="$(mktemp -d)"
  pier="$tmpdir/pier"

  echo "=== legacy migration test: $name ==="

  tar xfz "$fixture" -C "$tmpdir"

  # Find the unpacked pier (first directory inside tmpdir).
  pier=$(find "$tmpdir" -mindepth 1 -maxdepth 1 -type d | head -1)
  if [ -z "$pier" ]; then
    echo "FAILED: could not find pier directory in $fixture" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  # Capture stderr to check for migration errors.
  stderr_log="$tmpdir/vere-stderr.log"

  set +e
  "$vere32" --lite-boot --daemon "$pier" 2>"$stderr_log"
  boot_exit=$?
  set -e

  if [ $boot_exit -ne 0 ]; then
    echo "FAILED: $name: vere exited $boot_exit during boot" >&2
    cat "$stderr_log" >&2
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  if ! wait_for_port "$pier"; then
    echo "FAILED: $name: ship did not come up" >&2
    cat "$stderr_log" >&2
    if [ -f "$pier/.vere.lock" ]; then
      kill "$(< "$pier/.vere.lock")" 2>/dev/null || true
    fi
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

  if ! [ 3 -eq "$(lensd "$port" 3)" ]; then
    echo "FAILED: $name: dojo sanity check failed" >&2
    lensa "$port" hood '+hood/exit' || true
    wait_for_shutdown "$pier" || true
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  if grep -qE "(failed to migrate|migration error)" "$stderr_log"; then
    echo "FAILED: $name: migration error in stderr" >&2
    cat "$stderr_log" >&2
    lensa "$port" hood '+hood/exit' || true
    wait_for_shutdown "$pier" || true
    ((failures++))
    rm -rf "$tmpdir"
    continue
  fi

  # Check arvo state mug hash against golden file if present.
  computed_mug=$(lensd "$port" '(mug .(now 0, eny 0))')
  mug_file="$fixtures_dir/$name.mug"
  if [ -f "$mug_file" ]; then
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
  else
    echo "$name: no golden mug file; computed mug is $computed_mug" >&2
    echo "$name: to record it: echo $computed_mug > tests/fixtures/$name.mug" >&2
  fi

  lensa "$port" hood '+hood/exit'
  wait_for_shutdown "$pier"

  echo "=== $name: migration succeeded ==="
  rm -rf "$tmpdir"
done

if [ $failures -gt 0 ]; then
  echo "test-legacy: $failures fixture(s) failed" >&2
  exit 1
fi

echo "test-legacy: all fixtures passed"
