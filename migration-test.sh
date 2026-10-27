#!/bin/bash
# migration-test.sh: roundtrip migration test for the vere loom.
#
# Tests the 32↔64-bit loom migration path by running three sequential boots on
# the same fake pier:
#   1. 32-bit vere: fresh boot from pill → saves a 32-bit loom snapshot
#   2. 64-bit vere: re-boots same pier   → migrates 32→64 (u3_migrate_64)
#   3. 32-bit vere: re-boots same pier   → migrates 64→32 (u3_migrate_32)
#
# Required environment variables:
#   VERE32_BINARY   32-bit vere binary path, relative to GITHUB_WORKSPACE or PWD
#   VERE64_BINARY   64-bit vere binary path, relative to GITHUB_WORKSPACE or PWD

set -xeuo pipefail

workspace=${GITHUB_WORKSPACE:-$(pwd)}

#  on a windows runner both GITHUB_WORKSPACE and git-bash's pwd hand back
#  a drive-letter path with backslashes ("D:\a\vere\vere"). most of the
#  tools below cope, but gnu tar reads a leading "D:" as a remote
#  host:path and tries to resolve the host "D". convert once, here, so
#  every path derived from it is msys-native.
#
case "$workspace" in
  [A-Za-z]:[\\/]*) workspace=$(cygpath -u "$workspace") ;;
esac

vere32="$workspace/$VERE32_BINARY"
vere64="$workspace/$VERE64_BINARY"
brass_pill="$workspace/brass.pill"
pier="$workspace/pier-migration"
arvo_dir="$workspace/urbit"

# Pinned commit used by boot-fake-ship.sh.
ARVO_COMMIT=592b957a30b302cb7ae7fea78c6804c9d63d97ef

# ── Fetch dependencies if not already present ─────────────────────────────────

if [ ! -f "$brass_pill" ]; then
  curl -LJ -o "$brass_pill" \
    "https://github.com/urbit/urbit/raw/${ARVO_COMMIT}/bin/brass.pill"
fi

if [ ! -d "$arvo_dir" ]; then
  curl -LJ -o "$workspace/urbit.tar.gz" \
    "https://github.com/urbit/urbit/archive/${ARVO_COMMIT}.tar.gz"
  mkdir "$arvo_dir"

  #  git-bash's tar cannot get this archive out in one pass. it wants a
  #  symlink's target to exist before it will create the link -- but only
  #  for a target that stays inside the extraction tree, so of the 262
  #  links the 217 beginning "../" are made regardless, and of the 45
  #  left, the 8 whose target sorts after them fail with ENOENT
  #  (pkg/arvo/gen/cat.hoon -> clay/cat.hoon, and seven like it). a
  #  second pass finds every target in place and makes all 8 -- but then
  #  exits nonzero itself, over the four symlinks-to-directories the
  #  first pass did create (pkg/arvo/lib/verb and three like it), which
  #  it will not extract over.
  #
  #  so neither pass can be judged by its exit status. run both and
  #  check what landed. posix tar creates every link outright and is
  #  done in the first pass, with the second a no-op.
  #
  tar xfz "$workspace/urbit.tar.gz" -C "$arvo_dir" --strip-components=1 || true
  tar xfz "$workspace/urbit.tar.gz" -C "$arvo_dir" --strip-components=1 || true

  set +x
  missing=$(tar tzf "$workspace/urbit.tar.gz" | sed 's|^[^/]*/||' \
            | while read -r mem_p; do
                case "$mem_p" in ''|*/) continue ;; esac
                [ -e "$arvo_dir/$mem_p" ] || echo "$mem_p"
              done)
  set -x

  if [ -n "$missing" ]; then
    echo "ERROR: arvo extraction is incomplete, missing:" >&2
    echo "$missing" >&2
    exit 1
  fi

  cp -RL "$arvo_dir/tests" "$arvo_dir/pkg/arvo/tests"
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

# Wait for the HTTP port file to appear (up to ~2 minutes).
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

# Wait for the vere lock file to disappear (ship has shut down).
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

cleanup() {
  if [ -f "$pier/.vere.lock" ]; then
    kill "$(< "$pier/.vere.lock")" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# ── Step 1: 32-bit vere → create 32-bit snapshot ─────────────────────────────

echo "=== migration step 1: boot with 32-bit vere (create 32-bit snapshot) ==="

rm -rf "$pier"
"$vere32" --lite-boot --daemon --fake bus \
  --bootstrap "$brass_pill" \
  --arvo "$arvo_dir/pkg/arvo" \
  --pier "$pier"

wait_for_port "$pier"
port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

lensd "$port" '+vats %base'
[ 3 -eq "$(lensd "$port" 3)" ]

lensa "$port" hood '+hood/exit'
wait_for_shutdown "$pier"

echo "=== step 1 done: 32-bit snapshot saved ==="

# ── Step 2: 64-bit vere → migrate 32→64 ──────────────────────────────────────

echo "=== migration step 2: boot with 64-bit vere (32→64 migration) ==="

rm -f "$pier/.http.ports"
"$vere64" --lite-boot --daemon "$pier"

wait_for_port "$pier"
port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

lensd "$port" '+vats %base'
[ 3 -eq "$(lensd "$port" 3)" ]

lensa "$port" hood '+hood/exit'
wait_for_shutdown "$pier"

echo "=== step 2 done: 32→64 migration succeeded ==="

# ── Step 3: 32-bit vere → migrate 64→32 ──────────────────────────────────────

echo "=== migration step 3: boot with 32-bit vere (64→32 migration) ==="

rm -f "$pier/.http.ports"
"$vere32" --lite-boot --daemon "$pier"

wait_for_port "$pier"
port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

lensd "$port" '+vats %base'
[ 3 -eq "$(lensd "$port" 3)" ]

lensa "$port" hood '+hood/exit'
wait_for_shutdown "$pier"

echo "=== step 3 done: 64→32 migration succeeded ==="
echo "Migration roundtrip complete."
