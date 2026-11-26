#!/bin/bash
# unit-tests.sh: run the installed unit-test executables.
#
# `zig build test-bins` installs them; this runs the files. A zig run
# step is cached, so on a warm cache `zig build <name>-test` can report
# success without executing anything, and a cross-compiled test can't
# run where it was built at all.
#
# Usage: unit-tests.sh [dir]     (default: zig-out/bin)
#
# Runs every *-test in the directory but tracy-test (a demo that waits
# for a profiler), then the benchmarks as a smoke test, then soft-test
# again from an ivory pill that isn't canonicalized, so that home holds
# equal nouns at different addresses for inner roads to unify (a plain
# run canonicalizes the pill at boot).
#
# Every test runs, whatever the ones before it did; the exit status is
# the number that failed.

set -uo pipefail

dir=${1:-zig-out/bin}

shopt -s nullglob
bins=("$dir"/*-test "$dir"/*-test.exe)

if [ ${#bins[@]} -eq 0 ]; then
  echo "no test binaries in $dir -- nothing ran" >&2
  exit 1
fi

for b in "$dir"/benchmarks "$dir"/benchmarks.exe; do
  [ -f "$b" ] && bins+=("$b")
done

failed=()

run() {
  local name=$1; shift
  echo "== $name"
  if "$@"; then
    echo "== $name: ok"
  else
    echo "== $name: FAILED (exit $?)" >&2
    failed+=("$name")
  fi
}

soft=

for b in "${bins[@]}"; do
  name=$(basename "$b" .exe)
  case "$name" in
    tracy-test) continue ;;
    soft-test)  soft=$b ;;
  esac
  run "$name" "$b"
done

if [ -n "$soft" ]; then
  run "soft-test (uncanonicalized pill)" env SOFT_TEST_RAW=1 "$soft"
fi

if [ ${#failed[@]} -gt 0 ]; then
  echo "unit-tests: ${#failed[@]} failed:" >&2
  printf '  %s\n' "${failed[@]}" >&2
  exit ${#failed[@]}
fi

echo "unit-tests: all passed"
