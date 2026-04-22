# Finding 019 — `u3qc_cut` 32-bit overflow → unbounded slab SEGV

**Status:** root-caused and fixed at `pkg/noun/jets/c/cut.c:33`.

**Severity:** medium. Memory-safety bug: any caller that passes an
attacker-controlled `c` (bit-count) to `u3qc_cut` can drive it into a
multi-hundred-MB `u3i_slab_init` request. On a 64 MiB fuzz loom this
SEGVs inside `_alloc_pages`; on a production 16 GiB loom the jet
still allocates ~512 MB before returning — usable as a DoS amplifier
if reachable through untrusted input.

The only observed path in the fuzz fleet is through `u3qe_rub`, which
decodes a length prefix from input bits and hands it to cut without
bounding; any other jet that builds `c` from untrusted data is
equally affected.

## Root cause

`pkg/noun/jets/c/cut.c:30-35` (pre-fix):

```c
if ( (0 == c_w) || (b_w >= len_w) ) {
  return 0;
}
if ( b_w + c_w > len_w ) {
  c_w = (len_w - b_w);
}
```

`b_w` and `c_w` are `c3_w` (uint32_t). The sum `b_w + c_w` wraps
modulo 2^32. For the observed crash input, `b_w = 64` and
`c_w = 0xffffffff`; the sum wraps to `0x3f = 63`, which is less than
`len_w = 72`, so the clamp never fires. `u3i_slab_init` is then
called with `c_w = 4_294_967_295` bits ≈ 512 MB, overflowing the
loom.

## How rub reaches this

`pkg/noun/jets/e/rub.c:51-65`:

```c
x = u3qa_dec(c);          // c from bit-scan: leading-zero count
y = u3qc_bex(x);          // 2^(c-1)  — can be 2^31 for 32-bit c
z = u3qc_cut(0, d, x, b); // read (c-1) more bits
e = u3qa_add(y, z);       // e ≈ 2^31 + something
...
q = u3qc_cut(0, z, e, b); // <-- passes e (~4 GB of bits) as c_w
```

For the repro bytes `b = 0xc0ffffffff00000000` (9 bytes, 72 bits):

1. Scan finds first 1-bit at position 32 → `c = 32`.
2. `x = 31`, `y = bex(31) = 2^31`.
3. `z = cut(0, 33, 31, b)` = 31 high bits of b = `0x7fffffff`.
4. `e = 2^31 + (2^31 - 1) = 0xffffffff`.
5. `q = cut(0, 64, 0xffffffff, b)` → cut's overflow → `slab_init(..., 0xffffffff)` → SEGV.

## Fix

One-line change to avoid the wrap:

```c
// pkg/noun/jets/c/cut.c:33
-      if ( b_w + c_w > len_w ) {
+      if ( c_w > (len_w - b_w) ) {
         c_w = (len_w - b_w);
       }
```

The preceding check `(b_w >= len_w) → return 0` guarantees
`len_w > b_w`, so `len_w - b_w` cannot underflow. Once clamped,
`c_w ≤ len_w - b_w ≤ len_w`, and `u3i_slab_init` gets a size bounded
by the source atom's actual bit-length.

Verified: both repros return exit 0 after the fix. The rub roundtrip
suite (`rub_roundtrip.afl`, 4096 test cases covering every valid
`mat`-encoded atom in `[0, 4096)`) still passes 4096/4096 — the fix
doesn't regress valid-input behavior.

## Why fix cut, not rub

Cut is the shared primitive. Every jet that uses it with a
bit-count derived from untrusted input is protected by the clamp
fix. Patching rub alone would leave identical bugs in any future
jet that makes the same mistake, and would not close the
memory-safety hole in `cut` itself.

## Reproducing (pre-fix)

```bash
./fuzz/build.sh fuzz_jet_l4b_encode
./fuzz/out/fuzz_jet_l4b_encode.afl \
  < fuzz/findings/019-jet_rub_cut_unbounded_slab_segv/repro1.bin
# Observed (pre-fix):
#   SIGSEGV in _alloc_pages (palloc.c:306), _ci_slab_init asked
#   for 4_294_967_295 bits.
```

Post-fix the harness runs cleanly; rub either produces a well-formed
`[p q]` pair or the cut-clamp reduces the request to the available
72 bits.

## Relationship to other findings

- **#015** (`zlib_decompress_bomb`): same general shape — unbounded
  input-driven allocation. Fixed at a different layer.
- **#016** (`u3qe_rub` mismatch): a separate off-by-one in rub's
  bit-offset math. Adversarial-only. This finding does NOT fix #016;
  they are independent.
