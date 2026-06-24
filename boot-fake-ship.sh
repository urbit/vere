#!/bin/bash

set -xeuo pipefail

urbit_binary=$GITHUB_WORKSPACE/$URBIT_BINARY
brass_pill=$GITHUB_WORKSPACE/brass.pill

curl -LJ -o $brass_pill https://github.com/urbit/urbit/raw/88c6173048d61ebd86455f0c1a8ce8f8099cbe01/bin/brass.pill
curl -LJ -o urbit.tar.gz https://github.com/urbit/urbit/archive/88c6173048d61ebd86455f0c1a8ce8f8099cbe01.tar.gz

mkdir ./urbit
tar xfz urbit.tar.gz -C ./urbit --strip-components=1
cp -RL ./urbit/tests ./urbit/pkg/arvo/tests

$urbit_binary --lite-boot --daemon --fake bus \
  --bootstrap $brass_pill                     \
  --arvo ./urbit/pkg/arvo                     \
  --pier ./pier

cleanup() {
  if [ -f ./pier/.vere.lock ]; then
    kill $(< ./pier/.vere.lock) || true
  fi
  set +x
}

trap cleanup EXIT
port=$(grep loopback ./pier/.http.ports | awk -F ' ' '{print $1}')

lensd() {
  curl -s                                                              \
    --data "{\"source\":{\"dojo\":\"$1\"},\"sink\":{\"stdout\":null}}" \
    "http://localhost:$port" | xargs printf %s | sed 's/\\n/\n/g'
}

lensa() {
  curl -s                                                             \
    --data "{\"source\":{\"dojo\":\"$2\"},\"sink\":{\"app\":\"$1\"}}" \
    "http://localhost:$port" | xargs printf %s | sed 's/\\n/\n/g'
}

check() {
  [ 3 -eq $(lensd 3) ]
}

lensd '+vat %base'

if check && sleep 10 && check; then
  echo "boot success"
  lensa hood '+hood/exit'
  while [ -f ./pier/.vere.lock ]; do
    echo "waiting for pier to shut down"
    sleep 5
  done
else
  echo "boot failure"
  kill $(< ./pier/.vere.lock) || true
  set +x
  exit 1
fi
