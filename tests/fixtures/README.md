# Migration test fixtures

Compressed fake-ship pier archives, each one saved by a specific old vere
release, plus a golden Arvo-state mug for each.  They back three scripts:
`test-legacy.sh`, `test-yolo-replay.sh`, and (indirectly) the roundtrip in
`migration-test.sh` at the repository root.

The archives are stored in Git LFS (see `.gitattributes`) and total about
1 GB, so a checkout needs `lfs: true` to get anything but pointer files.

## Inventory

| fixture | created with | event log layout | snapshot | size | golden mug |
|---|---|---|---|---|---|
| `zod-v1.21` | vere 1.21 | flat `.urb/log/data.mdb` (pre-epoch) | `chk/{north,south}.bin` | 197 MB | `1.875.565.524` |
| `zod-v2.12` | vere 2.12 | flat `.urb/log/data.mdb` (pre-epoch) | `chk/{north,south}.bin` | 232 MB | `1.222.863.659` |
| `zod-v3.3` | vere 3.3 | epochs `0i0` **and** `0i101` | `chk/{north,south}.bin` | 342 MB | `499.833.433` |
| `zod-v4.2` | vere 4.2 | single epoch `0i0` | `chk/image.bin` | 209 MB | `738.931.621` |

`zod-v1.21` and `zod-v2.12` also carry a `.urb/bhk/` snapshot backup.

Note that `zod-v4.2` is **not** a legacy-loom fixture: vere 4.x already
writes the current `palloc` loom, and its `image.bin` reads as
`ver_d = 5, pam_d = 84` — U3V_VER5, 32-bit, `u3a_vits = 2`, 16K pages.  It
exercises the 32-bit-to-64-bit loom migration, not the v1-v4 chain.  The
three older fixtures are the ones that drive `_disk_migrate_loom`'s
`U3V_VER1 -> VER2 -> VER3 -> VER4 -> VER5` fallthrough in `pkg/vere/disk.c`.

## The `.mug` files

Each `<name>.mug` holds the value of `(mug .(now 0, eny 0))` for that pier —
a hash of the whole Arvo state with the two sources of nondeterminism zeroed.
`test-yolo-replay.sh` replays the pier from event 1 and compares against it.

This is the strongest correctness evidence in the 64-bit work: the same
golden value must come back from a 32-bit **and** a 64-bit binary, which is a
direct check that both bitnesses compute identical Arvo state from identical
event logs.  CI runs the replay once per bitness for exactly that reason.

## `test-legacy.sh`

Migration smoke test.  Globs **every** `*.tar.gz` in this directory, and for
each one:

1. Unpacks the pier into a temporary directory.
2. Boots current 32-bit vere on it, which detects the old loom format and
   runs the migration chain automatically.
3. Verifies the ship answers a dojo command (`[ 3 -eq $(lensd 3) ]`).
4. Sends `+hood/exit` and waits for a clean shutdown.
5. Fails if vere exits non-zero or migration errors appear on stderr.

Needs `VERE32_BINARY`.  If no archives are present (an LFS-less checkout) it
prints a notice and exits 0 rather than failing.

## `test-yolo-replay.sh`

Replay test with **no disk migration at all**.  For each eligible fixture:

1. Unpacks the pier and checks the layout is replayable in place — flat, or a
   single `0i0` epoch.  Hard-fails on anything else.
2. Deletes `.urb/chk/*` so replay starts from event 1 rather than from the
   existing snapshot.
3. Runs `vere play -f -y --no-migrate <pier>`.
4. Boots with `--lite-boot --daemon` and no injected events.
5. Compares `(mug .(now 0, eny 0))` against the committed `.mug`.
6. Greps the captured stderr for migration strings; any hit fails the test.

Needs `VERE_BINARY`; the caller runs it once per bitness.

### Eligibility

`ELIGIBLE` in the script is an explicit allowlist — currently `zod-v1.21` and
`zod-v2.12`.  Adding a fixture without auditing its layout is deliberately a
two-step operation.  The two exclusions:

- **`zod-v3.3`** spans epochs `0i0` and `0i101`, and replay-from-event-1 is
  not well-defined across multiple epochs.
- **`zod-v4.2`** has a non-replayable boot sequence: events 1-5 bail during
  `u3v_boot` (ride compiles, then aborts) under *any* vere, including the
  4.2 binary that originally wrote them.  This looks like an OTA or
  imported-state artifact, not a runtime regression — in particular it is
  **not** a vere64 problem.

## CI

`.github/workflows/shared.yml`, in the `migration-test` job:

| step | script | binaries |
|---|---|---|
| Run 32↔64 roundtrip migration test | `migration-test.sh` | both |
| Run legacy v1–v4 migration tests | `test-legacy.sh` | 32-bit |
| Run yolo-replay tests (32-bit) | `test-yolo-replay.sh` | 32-bit |
| Run yolo-replay tests (64-bit) | `test-yolo-replay.sh` | 64-bit |

## Creating a fixture

Generated once with the appropriate old vere release, then committed here.
Needs a Linux x86_64 environment.

```bash
# 1. Download the old vere release binary for the loom format you want.
#      https://bootstrap.urbit.org/vere32/edge/vX.Y.Z/
#    or, for very old releases:
#      https://bootstrap.urbit.org/vere/

OLD_VERE=./vere32-vX.Y.Z-linux-x86_64
chmod +x $OLD_VERE

# 2. Download brass.pill (same commit as boot-fake-ship.sh).
ARVO_COMMIT=592b957a30b302cb7ae7fea78c6804c9d63d97ef
curl -LJ -o brass.pill \
  "https://github.com/urbit/urbit/raw/${ARVO_COMMIT}/bin/brass.pill"
curl -LJ -o urbit.tar.gz \
  "https://github.com/urbit/urbit/archive/${ARVO_COMMIT}.tar.gz"
mkdir urbit && tar xfz urbit.tar.gz -C urbit --strip-components=1

# 3. Boot a fresh fake ship.
$OLD_VERE --lite-boot --daemon --fake zod \
  --bootstrap ./brass.pill \
  --arvo ./urbit/pkg/arvo \
  --pier ./zod-X.Y

# 4. Once .http.ports appears, capture the golden mug BEFORE shutting down.
PORT=$(grep loopback ./zod-X.Y/.http.ports | awk '{print $1}')
curl -s --data '{"source":{"dojo":"(mug .(now 0, eny 0))"},"sink":{"stdout":null}}' \
  "http://localhost:$PORT" | xargs printf %s | sed 's/\\n/\n/g' > zod-vX.Y.mug
cat zod-vX.Y.mug   # sanity-check: a dotted decimal like 738.931.621

# 5. Shut down cleanly so the snapshot is written.
curl -s --data '{"source":{"dojo":"+hood/exit"},"sink":{"app":"hood"}}' \
  "http://localhost:$PORT"
while [ -f ./zod-X.Y/.vere.lock ]; do sleep 2; done

# 6. Archive the pier.  Keep the directory inside the tarball named
#    zod-X.Y (no leading "v"), matching the existing fixtures.
tar czf zod-vX.Y.tar.gz --exclude='zod-X.Y/.vere.lock' zod-X.Y

# 7. Commit.  *.tar.gz here is already an LFS pattern via .gitattributes,
#    so `git add` stores a pointer -- confirm before pushing.
git add zod-vX.Y.tar.gz zod-vX.Y.mug
git check-attr filter zod-vX.Y.tar.gz   # -> filter: lfs
```

Adding a fixture to `test-yolo-replay.sh`'s `ELIGIBLE` list additionally
requires that its log be flat or a single `0i0` epoch, and that its events
actually replay from 1 — see the exclusions above.
