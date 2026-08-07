# Migration test fixtures

This directory holds compressed fake-ship pier archives used by
`test-legacy.sh` to exercise the v1–v4 loom migration chain.

## What is a fixture?

Each fixture is a `tar.gz` of a minimal fake-ship pier that was last saved
by a specific old vere release.  When `test-legacy.sh` boots the pier with
current 32-bit vere, the engine detects the old loom format and runs the
migration chain automatically (via `_disk_migrate_loom` in `disk.c`).

## Fixture inventory

| File | Created with | Loom version | Disk format |
|------|-------------|--------------|-------------|
| `v1-loom.tar.gz` | vere 1.x | U3V_VER1 | U3D_VER1 (no epochs) |
| `v2-loom.tar.gz` | vere 1.x after v2 migration | U3V_VER2 | U3D_VER1 |
| `v3-loom.tar.gz` | vere 2.x | U3V_VER3 | U3E_VER1 (epoch, old loom) |
| `v4-loom.tar.gz` | vere 3.x | U3V_VER4 | U3E_VER1 |

> These fixtures have not been committed yet.  See "Creating fixtures" below.

## Creating fixtures

Fixtures are generated once using the appropriate old vere release, then
committed to this directory.  You need a Linux x86_64 environment.

### General procedure

```bash
# 1. Download the old vere release binary for the desired loom version.
#    Releases are archived at:
#      https://bootstrap.urbit.org/vere32/edge/vX.Y.Z/
#    or (for very old releases):
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

# 3. Boot a fresh fake ship, then shut it down cleanly.
$OLD_VERE --lite-boot --daemon --fake bus \
  --bootstrap ./brass.pill \
  --arvo ./urbit/pkg/arvo \
  --pier ./fixture-pier

# Wait for the ship to boot (watch for .http.ports), then:
PORT=$(grep loopback ./fixture-pier/.http.ports | awk '{print $1}')
curl -s --data '{"source":{"dojo":"+hood/exit"},"sink":{"app":"hood"}}' \
  "http://localhost:$PORT"

# Wait for .vere.lock to disappear (clean shutdown / snapshot saved).
while [ -f ./fixture-pier/.vere.lock ]; do sleep 2; done

# 4. Archive the pier (exclude the lock file just in case).
tar czf vN-loom.tar.gz --exclude='fixture-pier/.vere.lock' fixture-pier

# 5. Verify the archive, then commit it here.
ls -lh vN-loom.tar.gz
```

### Version-specific notes

**U3V_VER1 / U3D_VER1** (no epoch directory, north+south loom):
- Use the oldest available vere 1.x release.
- The pier has `.urb/log/data.mdb` and `.urb/chk/north.bin` + `south.bin`.

**U3V_VER3–VER4 / U3E_VER1** (epoch directory, north+south loom):
- Use a vere 2.x or 3.x release that writes the epoch format with old loom.
- The pier has `.com/0i<N>/data.mdb` and `.urb/chk/north.bin` + `south.bin`.

## What test-legacy.sh checks

For each fixture the script:
1. Unpacks the pier into a temporary directory.
2. Boots current 32-bit vere on it (`--lite-boot --daemon`).
3. Verifies the ship responds to `[ 3 -eq $(lensd 3) ]`.
4. Sends `+hood/exit` and waits for a clean shutdown.
5. Fails if vere exits non-zero or migration error messages appear in stderr.
