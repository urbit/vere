#!/bin/bash

set -xeuo pipefail

urbit_binary=$GITHUB_WORKSPACE/$URBIT_BINARY
brass_pill=$GITHUB_WORKSPACE/brass.pill

curl -LJ -o $brass_pill https://github.com/urbit/urbit/raw/592b957a30b302cb7ae7fea78c6804c9d63d97ef/bin/brass.pill
curl -LJ -o urbit.tar.gz https://github.com/urbit/urbit/archive/592b957a30b302cb7ae7fea78c6804c9d63d97ef.tar.gz

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
