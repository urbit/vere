# Finding 003 — `ur_cue`: zero-length iatom causes heap-buffer-overflow in jam

**Status:** FIXED in `pkg/ur/serial.c` `_cue_next`: after the strip-
trailing-zeros loop, a `len_byt == 0` result now collapses the atom
to the direct atom zero and frees the intermediate buffer, instead of
calling `ur_coin_bytes_unsafe(r, 0, byt)` and later tripping
`ur_met0_bytes_unsafe` via `len - 1` underflow.

Regression reproducer: `fuzz/regression/fuzz_ur_cue/003-zero-len-iatom-oob.bin`
and `fuzz/regression/fuzz_ur_jam_cue_diff/003-zero-len-iatom-oob.bin`.

**Severity:** out-of-bounds read (1 byte, before the heap allocation).
Reachable only via pkg/ur direct consumers — **not** via Vere's
production ingest paths (they use `pkg/noun/serial.c`'s own cue).

## Trigger

12-byte crafted jam input:

```
00000000: 0001 0000 0000 0000 0000 4b18            ..........K.
```

## Root cause

1. `_cue_next` at `pkg/ur/serial.c:234-258` takes the indirect-atom
   branch when `len > 62` (claimed bit length).
2. `calloc(len_byt, 1)` produces a byte buffer that `ur_bsr_bytes_any`
   fills from the bitstream.
3. The strip-trailing-zeros loop:
   ```c
   while ( len_byt && !byt[len_byt - 1] ) {
     len_byt--;
   }
   ```
   can reduce `len_byt` to **0** when all bytes in the buffer are zero.
4. Cue then calls `ur_coin_bytes_unsafe(r, 0, byt)`, creating an iatom
   with `len = 0` in the root's atoms table.
5. Later, jam walks the noun:
   - `_jam_atom` calls `ur_met(r, 0, ref)` at `serial.c:51`.
   - `ur_met` for an iatom (`hashcons.c:562`) calls
     `ur_met0_bytes_unsafe(len=0, byt)`.
6. `ur_met0_bytes_unsafe` at `pkg/ur/defs.h:75-80`:
   ```c
   inline uint64_t
   ur_met0_bytes_unsafe(uint64_t len, uint8_t *byt)
   {
     uint64_t last = len - 1;
     return (last << 3) + ur_met0_8(byt[last]);
   }
   ```
   With `len = 0`, `last = UINT64_MAX`. `byt[UINT64_MAX]` wraps the
   pointer arithmetic and reads 1 byte before the allocated buffer —
   heap-buffer-overflow.

The comment above `ur_met0_bytes_unsafe` at `defs.h:73` already
warns:

> unsafe wrt trailing null bytes, which are invalid

so the **precondition** for the helper is that `len > 0` and the
last byte is nonzero — cue violates both when the crafted input's
atom content is all zeros.

## ASan report (stack)

```
READ of size 1 at 0x60200000000f
  #0 ur_met0_bytes_unsafe  pkg/ur/defs.h:79
  #1 ur_met                pkg/ur/hashcons.c:562
  #2 _jam_atom             pkg/ur/serial.c:51
  #3 ur_walk_fore_with     pkg/ur/hashcons.c:980
  #4 _jam                  pkg/ur/serial.c:97
  #5 ur_jam                pkg/ur/serial.c:150

is located 1 bytes to the left of 8-byte region allocated by:
  #0 calloc
  #1 _cue_next             pkg/ur/serial.c:255
  #2 _cue                  pkg/ur/serial.c:310
  #3 ur_cue_with           pkg/ur/serial.c:372
```

## Suggested fix

Option A: reject zero-length iatoms in cue. After trimming:

```c
while ( len_byt && !byt[len_byt - 1] ) {
  len_byt--;
}

if (len_byt == 0) {
  free(byt);
  *out = (ur_nref)0;    // direct atom zero
  return ur_cue_good;
}
```

Option B: defend the helper itself. Not ideal — the helper is called
from many hot paths and a length check would slow them all down.

Option A is minimal and pairs naturally with the finding-002 fix
(both are missing canonicalisation / promotion paths in cue after
the trim loop).

## Blast radius

Same as finding #002: reachable only from direct `ur_cue` consumers
(`pkg/vere/benchmarks.c` and pkg/ur's own tests). Vere's real ingest
paths use `u3s_cue_bytes` / `u3s_cue_xeno` which bypass `ur_cue`
entirely and build atoms through `u3i_slab_mint` — that path has its
own canonicalisation and does not hit this bug.

## Reproducing

```bash
./fuzz/build.sh fuzz_ur_jam_cue_diff
./fuzz/out/fuzz_ur_jam_cue_diff.afl < fuzz/findings/003-ur_cue-zero-len-iatom-oob/repro.bin
```

Expect ASan `heap-buffer-overflow` at `ur_met0_bytes_unsafe`.
