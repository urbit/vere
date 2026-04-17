#!/usr/bin/env bash
# Stop all running afl-fuzz screen sessions started by run.sh.
#
# Uses the full "PID.name" identifier (not just the name) so
# duplicate session names from overlapping runs don't confuse
# `screen -S`.
set -uo pipefail
for full in $(screen -ls 2>/dev/null | awk '/\.afl-/ {print $1}'); do
  echo "killing $full"
  screen -S "$full" -X quit 2>/dev/null || true
done
# Belt-and-suspenders: kill any remaining afl-fuzz processes whose
# screen wrappers crashed or leaked.
pkill -9 afl-fuzz 2>/dev/null || true
# Clean out detached/dead sockets.
screen -wipe >/dev/null 2>&1 || true
