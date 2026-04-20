# Finding 001 — `ur_cue`: unbounded atom allocation from untrusted input

**Status:** first crash found in the H1 `fuzz_ur_cue` campaign (~10s of
fuzzing, ~1100 execs). Not yet triaged into an upstream issue.

**Severity:** denial-of-service via remote / crafted jam bytes.

**Class:** untrusted length accepted and passed to `calloc`, resulting in
out-of-memory abort inside `_oom()`.

## Where

- `pkg/ur/serial.c:243` — `uint8_t *byt = _oom("cue_next bytes", calloc(len_byt, 1));`
- `_oom` at `pkg/ur/serial.c:9–18` — aborts unconditionally on NULL.

## Trigger

Any jam-encoded input whose atom-length header claims more bytes than
the host can reasonably allocate.

Minimal reproducer captured by AFL++ (file: `repro.bin`, 37 bytes):

```
00000000: 0000 0000 0000 1000 0000 0000 0000 00ff  ................
00000010: ffff 7e10 0000 ff1a 0000 0000 00ff ff00  ..~.............
00000020: 0000 00e5 ff                             .....
```

Running the H1 harness with this input produces:

```
AddressSanitizer: requested allocation size 0x800000000000
  (0x800000001000 after adjustments for alignment, red zones etc.)
  exceeds maximum supported size of 0x10000000000 (thread T0)
  #0 __interceptor_calloc
  #1 _cue_next  pkg/ur/serial.c:243
  #2 _cue       pkg/ur/serial.c:298
  #3 ur_cue_with pkg/ur/serial.c:360
```

An attacker controlling the jam bytes can make `cue_next` request
arbitrarily large heap allocations. On Vere's 16 GiB-max loom,
*anything* claiming more than that is unambiguously bogus and should be
rejected at parse time, not passed to the allocator.

## Why it matters

`ur_cue` is the deserialisation primitive used by:

- `u3s_cue_bytes` / `u3s_cue_xeno` in `pkg/noun/serial.c`
- `u3_newt_decode` in `pkg/vere/newt.c` (IPC message bodies)
- `u3_disk_sift` in `pkg/vere/disk.c` (event log replay)
- `_conn_moor_poke` in `pkg/vere/io/conn.c` (khan socket API)
- `_ames_hear` in `pkg/vere/io/ames.c` (UDP packet bodies, indirectly)

So a single crafted jam blob can remotely DoS a running ship via any of
these channels. The event-log path is particularly interesting because
a corrupted log aborts replay at startup — the ship won't boot.

## Existing upstream bound

`_cue` already rejects inputs whose total *bit* length exceeds
`0x7ffffffffffffffULL` (2^59 bits, ≈64 PiB). The gap is that individual
atom lengths inside the stream are not bounded against the *remaining*
input length, so a 9-byte input can still claim a 64 PiB atom.

## Suggested fix

In `_cue_next` / `ur_bsr_rub_len`, before calling `calloc(len_byt, 1)`:

```c
/* an atom claim larger than the remaining bitstream can be
 * bogus-by-construction: it would need more source bits than exist. */
if (len > (bsr->bytes_left * 8 + bsr->bits_left)) {
    return ur_cue_meme;
}
```

plus a hard upper bound matching the loom limit:

```c
#define UR_CUE_MAX_ATOM_BYTES ((uint64_t)16 << 30)   /* 16 GiB loom ceiling */
if (len_byt > UR_CUE_MAX_ATOM_BYTES) {
    return ur_cue_meme;
}
```

Both checks belong in `pkg/ur/serial.c` so all upstream cue paths
inherit them.

## Reproducing

```bash
./fuzz/build.sh fuzz_ur_cue
./fuzz/out/fuzz_ur_cue.afl < fuzz/findings/001-ur_cue-alloc-size-too-big/repro.bin
```

Expect an ASan `allocation-size-too-big` abort.

## Next steps

- [ ] File upstream issue referencing this finding
- [ ] Add a minimised reproducer to `fuzz/regression/fuzz_ur_cue/`
- [ ] Land the size check in `_cue_next`
- [ ] Confirm the fuzzer no longer produces variants of this class after
      the fix
