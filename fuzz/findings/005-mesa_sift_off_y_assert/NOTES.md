# Finding 005 — `mesa_sift_pact_from_buf`: assertion failure on bit-misaligned state

**Status:** **FIXED by urbit/vere PR 998** ("mesa: fix issues
discovered by fuzz testing"). Verified locally: applying that PR
to the working tree, my saved `repro.bin` returns exit 0 instead
of asserting. PR 998 reorders `_sift_next` to check `err_c` before
the assertion, replaces `_mesa_sift_name`'s manual buffer pointer
arithmetic with a bounds-checked `_sift_next` call, and adds an
`!sif_u->err_c` guard around the trailing mug check in
`_mesa_sift_pact`.

PR 998 also exposes a sibling class of asserts in `_mesa_sift_name`
that we filed as finding #006.

**Severity:** assertion abort triggered by crafted bytes. Reachable
from any mesa packet ingest path, which makes this a **remote DoS**
on any urbit running mesa.

## Trigger

6-byte crafted input:

```
00000000: ee19 0000 e300                           ......
```

Reproduces:

```bash
./fuzz/build.sh fuzz_mesa_sift_pact
./fuzz/out/fuzz_mesa_sift_pact.afl < fuzz/findings/005-mesa_sift_off_y_assert/repro.bin
```

Output:

```
fuzz_mesa_sift_pact.afl: pkg/vere/io/mesa/pact.c:367:
  Assertion `sif_u->off_y == 0' failed.
```

## Root cause

`_sift_next` at `pkg/vere/io/mesa/pact.c:364`:

```c
static c3_y*
_sift_next(u3_sifter* sif_u, c3_w len_w)
{
  assert ( sif_u->off_y == 0 ); // ensure all bits were sifted
  ...
}
```

The sifter has a bit-cursor (`off_y`) that tracks how many bits of
the current byte have been consumed. Byte-aligned reads (via
`_sift_next` or `_sift_bytes`) assert that `off_y == 0` so the next
byte starts at a byte boundary.

The crafted input causes some prior parsing step to leave `off_y != 0`
when control reaches `_sift_ship` → `_sift_bytes` → `_sift_next`. The
assertion correctly catches the invariant violation, but assertion
aborts in production are denial-of-service: they kill the process.

## Stack trace

```
_sift_next            pkg/vere/io/mesa/pact.c:367
_sift_ship            pkg/vere/io/mesa/pact.c:546
_mesa_sift_name       pkg/vere/io/mesa/pact.c:649
_mesa_sift_poke_pact  pkg/vere/io/mesa/pact.c:838
_mesa_sift_pact       pkg/vere/io/mesa/pact.c:890
mesa_sift_pact_from_buf pkg/vere/io/mesa/pact.c:940
```

## Production impact

`mesa_sift_pact_from_buf` is called from the UDP receive path for
mesa packets. A remote peer can send these 6 bytes to any vere
instance with mesa enabled and crash it. No authentication required.

## Investigation needed

The fix should:
1. Replace the assert with a graceful `_sift_fail` that records an
   error and returns NULL, OR
2. Fix the upstream parser path that's leaving `off_y` non-zero
   before reaching the byte-aligned ship read.

Option (2) is the better fix because it preserves the invariant
elsewhere in the parser; the assert is documentation that "this
should never happen here".

The `_sift_pact` chain that leads here parses:
- the mesa head (4 bytes, may consume bits within the last byte)
- a name (which starts with the ship)

If the head's bit-consumption left a non-zero `off_y` and the name
parser doesn't re-align before reading ship bytes, that's the gap.

## Reproducing without the harness

The 6-byte input triggers the assert deterministically. A direct
test in `pact_test.c` would catch this if it were exercised on the
fuzzer's mutation set.
