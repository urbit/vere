#!/bin/bash
# stack-overflow-test.sh: a C stack overflow must bail, every time.
#
# Deep interpreter recursion exhausts the C stack. That has to produce
# `dig: over` and leave the ship running -- not once, but on every
# subsequent attempt.
#
# This is a regression test for two windows-only faults, neither of which
# has a POSIX analogue, and both of which look identical from the outside
# (`serf: unexpectedly shut down`):
#
#   - POSIX runs the overflow handler on a dedicated alternate stack.
#     windows runs a vectored handler on the stack that just overflowed,
#     so it needs headroom reserved up front (SetThreadStackGuarantee).
#     without it, the *first* overflow is already fatal.
#
#   - windows raises EXCEPTION_STACK_OVERFLOW once: catching it consumes
#     the thread's guard page and nothing puts it back, so the *second*
#     overflow has nothing to trip (_resetstkoflw restores it).
#
# Hence more than one run: a single pass cannot tell the second fault
# from a fix.
#
# Required environment variables:
#   URBIT_BINARY   runtime path, relative to GITHUB_WORKSPACE or PWD
#
# Optional:
#   PIER_DIR       pier to resume; booted fresh if absent
#   OVERFLOW_RUNS  attempts (default 3)

set -euo pipefail

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

urbit_binary="$workspace/$URBIT_BINARY"
pier=${PIER_DIR:-"$workspace/pier-overflow"}
runs=${OVERFLOW_RUNS:-3}
log="$workspace/overflow-output"

#  pinned by boot-fake-ship.sh
ARVO_COMMIT=592b957a30b302cb7ae7fea78c6804c9d63d97ef

#  recurses through turn, so each level nests a fresh set of interpreter
#  frames rather than tail-calling: the C stack goes before the loom does,
#  which is the point -- a loom exhaustion would bail meme instead.
#
snippet='=/  l=(list *)  ~[~]  |-  %+  turn  l  |=  i=*  ^$(l ~[i])'

: > "$log"
tail -F "$log" >&2 &
tailproc=$!

cleanup() {
  if [ -f "$pier/.vere.lock" ]; then
    kill "$(< "$pier/.vere.lock")" 2>/dev/null || true
  fi
  sleep 1
  kill "$tailproc" 2>/dev/null || true
}
trap cleanup EXIT

# ── boot or resume ────────────────────────────────────────────────────────────

#  the pier this step resumes is usually the one test-fake-ship.sh just
#  finished with, and that script's cleanup only SIGTERMs the runtime --
#  it does not wait for it. a shutdown takes a snapshot first, so the old
#  process can still hold the fcntl lock when we get here, and the resume
#  dies with `pier: locked by PID`. vere unlinks .vere.lock on release,
#  so the file's absence is the signal; a lingering file whose pid is
#  gone means an unclean kill, which is equally fine to resume over.
#
wait_unlocked() {
  local lock="$pier/.vere.lock" pid

  for _ in $(seq 1 120); do
    [ -f "$lock" ] || return 0

    pid=$(< "$lock") || true

    if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
      return 0
    fi

    sleep 1
  done

  echo "ERROR: $pier is still locked by PID $pid after 120s" >&2
  return 1
}

if [ -d "$pier" ]; then
  echo "=== resuming $pier ==="
  wait_unlocked

  #  vere removes .http.ports on a clean exit, but not if it was killed.
  #  a stale one would satisfy the wait below instantly and hand us a
  #  port from the previous run.
  #
  rm -f "$pier/.http.ports"

  "$urbit_binary" --lite-boot --daemon "$pier" >> "$log" 2>&1
else
  echo "=== booting a fake ship at $pier ==="

  brass_pill="$workspace/brass.pill"
  arvo_dir="$workspace/urbit"

  if [ ! -f "$brass_pill" ]; then
    curl -LJ -o "$brass_pill" \
      "https://github.com/urbit/urbit/raw/${ARVO_COMMIT}/bin/brass.pill"
  fi

  if [ ! -d "$arvo_dir" ]; then
    curl -LJ -o "$workspace/urbit.tar.gz" \
      "https://github.com/urbit/urbit/archive/${ARVO_COMMIT}.tar.gz"
    mkdir "$arvo_dir"

    #  git-bash's tar wants a symlink's target to exist before it will
    #  create the link -- but only for a target that stays inside the
    #  extraction tree. of the tarball's 262 links, the 217 whose target
    #  begins with "../" escape that check and are made regardless; of
    #  the 45 that remain, the 8 whose target sorts after them in the
    #  archive fail with ENOENT (pkg/arvo/gen/cat.hoon -> clay/cat.hoon,
    #  and seven like it). every target exists by the second pass. posix
    #  tar creates the links outright and gets it right the first time.
    #
    tar xfz "$workspace/urbit.tar.gz" -C "$arvo_dir" --strip-components=1 \
      || tar xfz "$workspace/urbit.tar.gz" -C "$arvo_dir" --strip-components=1
  fi

  "$urbit_binary" --lite-boot --daemon --fake bus \
    --bootstrap "$brass_pill" \
    --arvo "$arvo_dir/pkg/arvo" \
    --pier "$pier" >> "$log" 2>&1
fi

for i in $(seq 1 60); do
  if grep -q loopback "$pier/.http.ports" 2>/dev/null; then
    break
  fi
  sleep 2
done

if ! grep -q loopback "$pier/.http.ports" 2>/dev/null; then
  echo "ERROR: timed out waiting for the HTTP port" >&2
  exit 1
fi

port=$(grep loopback "$pier/.http.ports" | awk -F ' ' '{print $1}')

# ── helpers ───────────────────────────────────────────────────────────────────

#  NB: liveness is a lens request, not a pid check. git-bash's kill does
#  not reliably see a native windows pid, and .http.ports outlives the
#  process that wrote it -- both would report a dead ship as alive, which
#  is the one answer this test must never get wrong.
#
alive() {
  curl -sf --max-time 20 \
    --data '{"source":{"dojo":"(add 1 1)"},"sink":{"stdout":null}}' \
    "http://localhost:$port" > /dev/null 2>&1
}

overs() {
  local n
  n=$(grep -c "dig: over" "$log" 2>/dev/null || true)
  echo "${n:-0}"
}

spew() {
  echo "--- last 40 lines of runtime output ---" >&2
  tail -40 "$log" >&2 || true
}

# ── the test ──────────────────────────────────────────────────────────────────

echo "=== overflowing the C stack ${runs}x ==="

for i in $(seq 1 "$runs"); do
  before=$(overs)

  #  the dojo bails, so lens may answer with an error or not at all;
  #  the assertion is the runtime's output, not this response
  #
  curl -s --max-time 120 \
    --data "{\"source\":{\"dojo\":\"${snippet}\"},\"sink\":{\"stdout\":null}}" \
    "http://localhost:$port" > /dev/null || true

  for _ in $(seq 1 30); do
    if [ "$(overs)" -gt "$before" ]; then
      break
    fi
    sleep 1
  done

  after=$(overs)

  if [ "$after" -le "$before" ]; then
    echo "ERROR: run $i produced no 'dig: over' (count stayed at $before)" >&2
    alive || echo "ERROR: and the ship is gone -- the overflow was fatal" >&2
    spew
    exit 1
  fi

  if ! alive; then
    echo "ERROR: run $i reported 'dig: over' but the ship then died" >&2
    spew
    exit 1
  fi

  echo "=== run $i: dig: over, ship alive ==="
done

# ── the ship must still work afterward ────────────────────────────────────────

got=$(curl -s --max-time 60 \
  --data '{"source":{"dojo":"(add 2 2)"},"sink":{"stdout":null}}' \
  "http://localhost:$port" | tr -dc '0-9')

if [ "$got" != "4" ]; then
  echo "ERROR: ship unresponsive after ${runs} overflows (got '${got}')" >&2
  spew
  exit 1
fi

echo "=== ${runs} overflows survived, ship still computing ==="
