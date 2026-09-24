#!/bin/bash

set -xeuo pipefail

urbit_binary=$GITHUB_WORKSPACE/$URBIT_BINARY

#  stream all runtime output to the log as it accrues
#
#  NB: the runtime must not inherit the CI log pipe on stdout: it puts
#  the (shared) pipe in non-blocking mode, causing spurious write
#  errors in subsequent commands
#
touch urbit-output
tail -F urbit-output >&2 &
tailproc=$!

cleanup () {
  kill $(cat ./pier/.vere.lock) || true

  #  give tail a moment to flush before killing it
  #
  sleep 1
  kill "$tailproc" 2>/dev/null || true
}

trap cleanup EXIT

if ! $urbit_binary --lite-boot --daemon --gc-abort ./pier >> urbit-output 2>&1; then
  echo "ERROR: failed to boot fake ship" >&2
  exit 1
fi

port=$(grep loopback ./pier/.http.ports | awk -F ' ' '{print $1}')
pierpid=$(cat ./pier/.vere.lock)

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

#  print the arvo version
#
lensd '+vats %base'

#  measure initial memory usage
#
lensd '~&  ~  ~&  %init-mass-start  ~'
lensa hood '+hood/mass'
lensd '~&  ~  ~&  %init-mass-end  ~'

#  run the unit tests
#
lensd '~&  ~  ~&  %test-unit-start  ~'
lensd '-test %/tests ~'
lensd '~&  ~  ~&  %test-unit-end  ~'

#  use the :test app to build all agents, generators, and marks
#
lensa hood '+hood/start %test'

lensd '~&  ~  ~&  %test-agents-start  ~'
lensa test '%agents'
lensd '~&  ~  ~&  %test-agents-end  ~'

lensd '~&  ~  ~&  %test-generators-start  ~'
lensa test '%generators'
lensd '~&  ~  ~&  %test-generators-end  ~'

lensd '~&  ~  ~&  %test-marks-start  ~'
lensa test '%marks'
lensd '~&  ~  ~&  %test-marks-end  ~'

#  measure memory usage post tests
#
lensd '~&  ~  ~&  %test-mass-start  ~'
lensa hood '+hood/mass'
lensd '~&  ~  ~&  %test-mass-end  ~'

#  defragment the loom
#
lensd '~&  ~  ~&  %pack-start  ~'
lensa hood '+hood/pack'
lensd '~&  ~  ~&  %pack-end  ~'

#  reclaim space within arvo
#
lensd '~&  ~  ~&  %trim-start  ~'
lensa hood '+hood/trim'
lensd '~&  ~  ~&  %trim-end  ~'

#  measure memory usage pre |meld
#
lensd '~&  ~  ~&  %trim-mass-start  ~'
lensa hood '+hood/mass'
lensd '~&  ~  ~&  %trim-mass-end  ~'

#  globally deduplicate
#
lensd '~&  ~  ~&  %meld-start  ~'
lensa hood '+hood/meld'
lensd '~&  ~  ~&  %meld-end  ~'

#  measure memory usage post |meld
#
lensd '~&  ~  ~&  %meld-mass-start  ~'
lensa hood '+hood/mass'
lensd '~&  ~  ~&  %meld-mass-end  ~'

lensa hood '+hood/exit'

cleanup

# Collect output
cp urbit-output test-output-unit
cp urbit-output test-output-agents
cp urbit-output test-output-generators
cp urbit-output test-output-marks

if [[ $URBIT_BINARY == *"macos"* ]]; then
    sed -i '' '0,/test-unit-start/d'       test-output-unit
    sed -i '' '/test-unit-end/,$d'         test-output-unit

    sed -i '' '0,/test-agents-start/d'     test-output-agents
    sed -i '' '/test-agents-end/,$d'       test-output-agents

    sed -i '' '0,/test-generators-start/d' test-output-generators
    sed -i '' '/test-generators-end/,$d'   test-output-generators

    sed -i '' '0,/test-marks-start/d'      test-output-marks
    sed -i '' '/test-marks-end/,$d'        test-output-marks
else
    sed -i '0,/test-unit-start/d'          test-output-unit
    sed -i '/test-unit-end/,$d'            test-output-unit

    sed -i '0,/test-agents-start/d'        test-output-agents
    sed -i '/test-agents-end/,$d'          test-output-agents

    sed -i '0,/test-generators-start/d'    test-output-generators
    sed -i '/test-generators-end/,$d'      test-output-generators

    sed -i '0,/test-marks-start/d'         test-output-marks
    sed -i '/test-marks-end/,$d'           test-output-marks
fi

OUTDIR="$(pwd)/test-fake-ship-output"
mkdir -p $OUTDIR
cp test-output-* $OUTDIR

set +x

hdr () {
  echo =====$(sed 's/./=/g' <<< "$1")=====
  echo ==== $1 ====
  echo =====$(sed 's/./=/g' <<< "$1")=====
}

for f in $(find "$OUTDIR" -type f); do
  hdr "$(basename $f)"
  cat "$f" || true
done

fail=0

for f in $(find "$OUTDIR" -type f); do
  if egrep "((FAILED|CRASHED)|warn:) " $f >/dev/null; then
    if [[ $fail -eq 0 ]]; then
      hdr "Test Failures"
    fi

    echo "ERROR Test failure in $(basename $f)"

    fail=$((fail+1))
  fi
done

#  scan the full runtime output for fatal errors, which indicate a crash
#  even if every test that managed to run reported success
#
if egrep "(unexpectedly shut down|serf error:|work exit: status|Assertion ')" urbit-output >/dev/null; then
  hdr "Runtime Crash"

  echo "ERROR fatal runtime error in urbit-output"

  fail=$((fail+1))
fi

if [[ $fail -eq 0 ]]; then
  hdr "Success"
fi

exit "$fail"
