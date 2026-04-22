# Finding 019 — `u3qe_rub` → `u3qc_cut` unbounded slab SEGV

**Status:** real crash. Loom exhaustion + unguarded writable mapping
produces a SEGV inside `_alloc_pages` (pkg/noun/palloc.c:306) when
`rub` accepts a malformed atom and cascades a multi-hundred-MB slab
allocation through `cut`.

**Severity:** medium. Callable from any Hoon code that feeds
attacker-controlled bytes into `rub` or `cue` (jam's inverse).
Observed on L4b encode harness (ivory-pill scope, 64 MiB loom) at
iteration ~170k. In prod vere the loom is 16 GiB so the allocation
completes but still causes a multi-second stall and a large one-shot
memory spike — useful as a DoS amplifier even if the SEGV doesn't
reproduce on a full-sized loom.

## Repro

```bash
./fuzz/build.sh fuzz_jet_l4b_encode
./fuzz/out/fuzz_jet_l4b_encode.afl \
  < fuzz/findings/019-jet_rub_cut_unbounded_slab_segv/repro1.bin
```

Repro bytes (11 bytes): `0d 00 00 00 00 00 ff ff ff ff c0`

- `buf[0] = 0x0d` → mode = 13 % 7 = 6 (`rub`)
- Remaining bytes build `[a=0 b=@]` where `b` decodes as an atom
  whose high bits force a giant `c` in rub's bit-scan.

## Stack (from gdb)

```
#0  _alloc_pages (siz_w=32769)             palloc.c:306
#1  _imalloc (len_w=32769)                 palloc.c:541
#2  u3a_walloc (len_w=134217731)           allocate.c:237   ← ~512 MB
#3  _ci_slab_init (len_w=134217728)        imprison.c:52
#4  u3i_slab_bare (len_d=4294967295)       imprison.c:153   ← 2^32-1
#5  u3i_slab_init (len_d=4294967295)       imprison.c:127
#6  u3qc_cut (a=0, b=64, c=2151670792,
              d=2147998450)                jets/c/cut.c:41
#7  u3qe_rub (a=0, b=2147998450)           jets/e/rub.c:65
#8  u3we_rub (cor=...)                     jets/e/rub.c:83
```

## Root cause

In `u3qe_rub` (`pkg/noun/jets/e/rub.c:52-65`):

```c
x = u3qa_dec(c);          //  c is derived by scanning b for 1-bits
y = u3qc_bex(x);          //  2^(c-1) — potentially astronomical
z = u3qc_cut(0, d, x, b); //  cuts (c-1) bits starting at d
...
q = u3qc_cut(0, z, e, b); //  cuts e bits starting at z
```

`c` is the number of leading zeros of `b` (per the scan loop at
lines 29-40). When `b` has its high bits set but is otherwise very
long, `c` can reach billions. The loop only guards against `x > m`
where `m = a + met(b)` — with a 64-bit atom containing high bits
set, `m` itself is already ~2 billion, so the guard passes.

Then `e = bex(c-1) + cut(...)` produces a billion-bit value.
`p = 2c + e` and the final `cut(0, z, e, b)` is asked to extract
`e` bits (~2 billion) from `b`. `u3qc_cut`
(`pkg/noun/jets/c/cut.c:33-46`) has a `b_w + c_w > len_w` clamp
but still ends up allocating a 256 MB slab for the 2-billion-bit
output — exceeding the 64 MiB loom.

## Why this is a real bug (not just a harness artifact)

The oracle check is off here — this crashes BEFORE the Hoon side
runs, so ice-flip is irrelevant. The C jet alone segfaults on an
input that neither bails with `c3__exit` nor returns a result.

Per Urbit jet contract, a jet that segfaults instead of bailing is
non-spec. The Hoon formula would run to completion (over many
minutes) producing a billion-bit atom, or equivalently would run
out of memory in a controlled way — either outcome is distinct
from a SEGV.

## Mitigation options

1. **Cap `c` in `u3qe_rub`** before calling `bex`/`cut`. Any
   reasonable ceiling (say `c <= 2^32 / 2`) covers every input that
   could be produced by `mat` (the jam encoder), since mat never
   emits more than 64 bits of length prefix.
2. **Cap slab size in `u3i_slab_init`**. Return a bail if the
   requested size exceeds the loom's free pages. Broader fix that
   also shields `cut` when called from other paths.
3. **Validate the leading-zero count in the rub scan loop**. The
   current `x > m` guard is in the scan; add a hard ceiling on `c`
   after the scan exits.

## Relationship to #015, #016

- **#015** (`zlib_decompress_bomb`): same shape — unbounded slab
  growth via input-driven length. Fix is structurally similar.
- **#016** (`u3qe_rub` mismatch): same jet. The mismatch in #016 is
  a separate bug in the bit-offset math; this is a memory-safety
  bug in the size validation. They can be fixed independently.

## Current coverage

Hit by `fuzz_jet_l4b_encode` in ~47h cumulative across two sessions.
Two distinct reproducers; both share the `rub` → `cut` pathway with
different `b` atom shapes.
