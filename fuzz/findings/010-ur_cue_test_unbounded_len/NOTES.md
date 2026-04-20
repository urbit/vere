# Finding 010 — `_cue_test_next`: NULL-deref via unbounded length advancing the bit cursor past 2^62

**Status:** **FIXED** in `pkg/ur/serial.c` `_cue_test_next` (atom +
backref cases). Same class as #001 / #004 — missing bounds check on
the rub-encoded length — but with a different downstream symptom.

**Severity:** **LOW** (in production). `ur_cue_test` is only called
from `pkg/vere/benchmarks.c` — not from any of Vere's network, IPC,
or disk ingest paths. But still a real UB violation worth fixing.

## Trigger

68-byte input found by H2 (`fuzz_ur_cue_test`) at ~21 minutes into
the 30-minute soak.

```
00000000: 1517 b200 0000 0000 0000 0002 e28a d500
00000010: 0000 0000 0000 0000 0000 0000 0000 0000
00000020: 0015 17b2 d085 59b4 5f7c 4f10 d2d2 d2d2
00000030: d2d2 d2d2 d2d2 d2d2 d2d2 d2d2 d2d2 d2d2
00000040: d2d2 d2d2
```

UBSan (under `-fsanitize=undefined -fno-sanitize-recover=all`):

```
pkg/ur/hashcons.c:442:31: runtime error: member access within
  null pointer of type 'ur_root_t' (struct ur_root_s)
    #0 ur_nref_mug         pkg/ur/hashcons.c:442
    #1 ur_dict_put         pkg/ur/hashcons.c:307
    #2 _cue_test_next      pkg/ur/serial.c:492
    #3 _cue_test           pkg/ur/serial.c:543
    #4 ur_cue_test_with    pkg/ur/serial.c:585
```

## Root cause

`_cue_test_next` (`pkg/ur/serial.c:434`) is the parse-only variant of
cue used by `ur_cue_test_with`. It deliberately passes `(ur_root_t*)0`
to `ur_dict_get` / `ur_dict_put` because no real noun construction
happens — the dict is keyed on bit-cursor positions, not noun refs.
That works fine **as long as those keys never have iatom/icell tag
bits set** (which would make `ur_nref_mug` try to dereference the
NULL root).

Bit-cursor positions normally fit in 60-ish bits for any realistic
input, so the top-2-bit tag is always `00` (`ur_direct`) and
`ur_nref_mug` takes the safe `ur_mug64(ref)` branch.

The bug: nothing bounds-checks the `len` returned by
`ur_bsr_rub_len`. A crafted input encodes a backref or atom claim
of, say, 2^62 bits. The function then calls
`ur_bsr_skip_any(bsr, 2^62)`, which advances `bsr->bits` past 2^62.
On the next iteration of the cue loop, `bits = bsr->bits` snapshot
has the high tag bits set. `ur_dict_put((ur_root_t*)0, dict, bits)`
calls `ur_nref_mug(NULL, ref)` with `ur_nref_tag(ref) == ur_iatom`
(or `ur_icell`), and that dereferences NULL.

The other cue paths I fixed earlier (`#004`) DO have the bounds
check — but `_cue_test_next` was a parallel implementation that got
missed.

## The fix

Mirror the `#004` fix at both the backref and atom cases — reject
claims that exceed the remaining bitstream:

```c
case ur_jam_back: {
  ...
  else if ( 62 < len ) {
    return ur_cue_meme;
  }
  else if ( len > ((bsr->left << 3) - bsr->off) ) {  // <-- new
    return ur_cue_meme;
  }
  ...
}

case ur_jam_atom: {
  ...
  if ( len > ((bsr->left << 3) - bsr->off) ) {        // <-- new
    return ur_cue_meme;
  }
  ur_bsr_skip_any(bsr, len);
  ...
}
```

## Production impact

Minimal — `ur_cue_test` is not on any production ingest path. The
only callers in the entire tree are `pkg/vere/benchmarks.c:242, 266`.
The fix is still worth landing because:
1. It restores the same bounds-checking discipline as the other
   cue paths.
2. It removes a UB violation that ASan/UBSan would otherwise keep
   flagging as we expand fuzzing.
3. It pairs naturally with the `#001` / `#004` / `#008` family of
   cue-side bounds checks — same shape, same fix.

## Reproducing

```bash
./fuzz/build.sh fuzz_ur_cue_test
./fuzz/out/fuzz_ur_cue_test.afl < fuzz/findings/010-ur_cue_test_unbounded_len/repro.bin
```

Pre-fix: UBSan null-deref report.
Post-fix: clean exit.

## Found by

H2 `fuzz_ur_cue_test` 30-minute background soak. Found at
~21 minutes via splice mutation of two corpus entries.
