#!/bin/bash

set -xeuo pipefail

urbit_binary=$GITHUB_WORKSPACE/$URBIT_BINARY
brass_pill=$GITHUB_WORKSPACE/brass.pill

curl -LJ -o $brass_pill https://github.com/urbit/urbit/raw/88c6173048d61ebd86455f0c1a8ce8f8099cbe01/bin/brass.pill
curl -LJ -o urbit.tar.gz https://github.com/urbit/urbit/archive/88c6173048d61ebd86455f0c1a8ce8f8099cbe01.tar.gz

mkdir ./urbit
tar xfz urbit.tar.gz -C ./urbit --strip-components=1
cp -RL ./urbit/tests ./urbit/pkg/arvo/tests

#  stream all runtime output to the log as it accrues
#
#  NB: the runtime must not inherit the CI log pipe on stdout: it puts
#  the (shared) pipe in non-blocking mode, causing spurious write
#  errors in subsequent commands
#
touch urbit-output
tail -F urbit-output >&2 &
tailproc=$!

cleanup() {
  if [ -f ./pier/.vere.lock ]; then
    kill $(< ./pier/.vere.lock) || true
  fi

  #  give tail a moment to flush before killing it
  #
  sleep 1
  kill "$tailproc" 2>/dev/null || true
}

trap cleanup EXIT

$urbit_binary --lite-boot --daemon --fake bus \
  --bootstrap $brass_pill                     \
  --arvo ./urbit/pkg/arvo                     \
  --pier ./pier >> urbit-output 2>&1

port=$(grep loopback ./pier/.http.ports | awk -F ' ' '{print $1}')
pierpid=$(< ./pier/.vere.lock)

#  fail immediately if the urbit process is no longer running
#
check_ship() {
  if ! kill -0 "$pierpid" 2>/dev/null; then
    echo "ERROR: urbit process exited unexpectedly" >&2
    exit 1
  fi
}

#  NB: xargs chokes on quotes in the response; tolerate that (|| true),
#  failing the pipeline only if curl itself fails
#
lensd() {
  check_ship
  curl -s                                                              \
    --data "{\"source\":{\"dojo\":\"$1\"},\"sink\":{\"stdout\":null}}" \
    "http://localhost:$port" | { xargs printf %s | sed 's/\\n/\n/g' || true; } || {
      check_ship
      echo "ERROR: lens request failed" >&2
      exit 1
    }
}

lensa() {
  check_ship
  curl -s                                                             \
    --data "{\"source\":{\"dojo\":\"$2\"},\"sink\":{\"app\":\"$1\"}}" \
    "http://localhost:$port" | { xargs printf %s | sed 's/\\n/\n/g' || true; } || {
      check_ship
      echo "ERROR: lens request failed" >&2
      exit 1
    }
}

check() {
  [ 3 -eq $(lensd 3) ]
}

lensd '+vats %base'

if check && sleep 10 && check; then
  echo "boot success"
  lensa hood '+hood/exit'
  while [ -f ./pier/.vere.lock ]; do
    if ! kill -0 "$pierpid" 2>/dev/null; then
      sleep 1  # grace period: a clean exit removes the lock file
      [ -f ./pier/.vere.lock ] || break
      echo "ERROR: urbit process crashed during shutdown" >&2
      exit 1
    fi
    echo "waiting for pier to shut down"
    sleep 5
  done
else
  echo "boot failure"
  exit 1
fi
