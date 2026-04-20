# Finding 008 — `ur_bsr_log`: heap OOB read past end of bitstream

**Status:** **FIXED in `pkg/ur/bitstream.c:621-633`**. Off-by-one in
the leading-zero-scan loop.

**Severity:** **HIGH**. One-byte heap OOB read inside the cue
bitstream reader. **Reachable from every cue caller**: `u3s_cue_bytes`,
`u3s_cue_xeno`, `u3_disk_sift` (event log replay), conn IPC, ames
packet bodies, mesa packet bodies. A crafted jam'd payload sent over
any of these channels triggers the read. ASan detects it cleanly;
without ASan the read is silent unless it crosses a guard page.

## Root cause

`pkg/ur/bitstream.c` `ur_bsr_log` (the function that scans for the
leading 1-bit in a rub-encoded length) had this loop:

```c
while ( !byt ) {
  if ( 32 == skip ) {
    return _bsr_log_meme(bsr);
  }

  byt = b[++skip];           // ← read happens BEFORE bounds check

  if ( skip == left ) {
    return _bsr_set_gone(bsr, (skip << 3) - off);
  }
}
```

When the buffer is `N` bytes and the first `N` bytes are all zero,
the loop exits via the `skip == left` branch — but only AFTER reading
`b[N]`, which is one byte past the buffer. Always.

The OOB byte is whatever sits in memory after the input atom. For
loom-allocated atoms with palloc poisoning enabled (which is the
default in fuzz/ASan builds), the next byte is shadow-poisoned and
ASan reports `use-after-poison`. In production without ASan, the
read is silent unless the byte happens to be in unmapped memory.

## Trigger

Discovered by H11 (`fuzz_disk_sift`) within ~30 minutes of
fork-mode fuzzing. Saved reproducer (54 bytes):

```
00000000: 1271 7171 7171 7171 7171 7171 7171 7171
00000010: 7171 7171 7171 7171 7171 7171 7171 7171
00000020: 7171 7171 7171 7171 7171 7171 7171 7171
00000030: 7171 7126 3456 7871
```

Reproducing:

```bash
./fuzz/out/fuzz_disk_sift.afl < fuzz/findings/008-ur_bsr_log_oob/repro.bin
```

Pre-fix: ASan reports `use-after-poison` at `bitstream.c:626`.
Post-fix: clean exit.

## Stack trace (pre-fix)

```
ur_bsr_log              pkg/ur/bitstream.c:626
ur_bsr_rub_len          pkg/ur/bitstream.c:663
_cs_cue_bytes_next      pkg/noun/serial.c:788
u3s_cue_bytes           pkg/noun/serial.c:875
u3s_cue_atom            pkg/noun/serial.c:912
u3qe_cue                pkg/noun/jets/e/cue.c:12
u3ke_cue                pkg/noun/jets/e/cue.c:24
u3_disk_sift            pkg/vere/disk.c:331
```

## Production impact

Same impact as findings #001 / #004 / #007: every cue ingest path is
affected.

- `u3_disk_sift` — event log replay (corrupted log → boot crash)
- `u3s_cue_xeno*` paths — ames packet bodies, mesa packet bodies,
  conn IPC, khan socket
- `u3s_cue_atom` / `u3s_cue_bytes` — direct callers in vere

In production builds without ASan, the OOB read is "silent" — it
returns whatever byte happens to be next to the atom in the loom.
That byte will frequently be a palloc box header (8-byte word
metadata). The function then either treats it as a valid
length-encoding bit (and produces a wildly wrong noun) or trips
some downstream check. So while not always immediately crashing,
it's a *correctness* bug in addition to a memory-safety one.

## The fix

Reorder: increment `skip`, check bounds, THEN read.

```c
while ( !byt ) {
  if ( 32 == skip ) {
    return _bsr_log_meme(bsr);
  }

  ++skip;
  if ( skip == left ) {
    return _bsr_set_gone(bsr, (skip << 3) - off);
  }
  byt = b[skip];
}
```

This preserves the semantics — the function still returns
`_bsr_set_gone` with the same `(skip << 3) - off` value — but the
OOB read is gone.

## Notes

- Found by H11 because the disk_sift path uses `u3ke_cue` which goes
  through the *u3* cue (`_cs_cue_bytes_next`), which calls
  `ur_bsr_rub_len` → `ur_bsr_log`. The bug lives in the **shared**
  pkg/ur bitstream reader, so every cue path inherits it.
- Multiple background sessions independently re-discovered this bug
  during the 4-minute test run. AFL converged on it quickly because
  havoc mutations easily produce sequences of leading zeros.
- Same class as #001, #004, #007: cue accepting bytes it shouldn't.
  But this one is a DIRECT memory-safety bug rather than an
  unbounded-allocation issue.
