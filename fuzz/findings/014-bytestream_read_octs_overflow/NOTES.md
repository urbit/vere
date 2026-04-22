# Finding 014 — `_qe_bytestream_read_octs` integer overflow → OOB read

**Status:** real bug, not yet patched.
**Severity:** remote-reachable crash (out-of-bounds read, potentially exploitable info-leak).
**Reachability:** any Hoon peer code that can call `+read-octs` on a bytestream jet with attacker-controlled `pos` and `n` — which is the intended jet signature.

## The bug

`pkg/noun/jets/e/bytestream.c:567`:

```c
if (pos_w + n_w > p_octs_w) {
  u3m_bail(c3__exit);
}
```

`pos_w` and `n_w` are `c3_w` (uint32_t), both derived from untrusted atoms via `u3r_safe_word`. The sum `pos_w + n_w` wraps silently on overflow, allowing values that SHOULD bail to slip past the guard.

Input that trips it:
- `pos_w = 0xFFFFFFFF` (4294967295)
- `n_w   = 4096`
- `pos_w + n_w = 0xFFFFFFFF + 0x1000 = 0xFFF` (4095), which is smaller than `p_octs_w` (e.g. 4096). **Guard passes.**

Then at line 575 the same pattern:
```c
if (pos_w + n_w > len_w) { ... }
```
also wraps. So `red_w` stays at `n_w` (4096).

Line 596:
```c
memcpy(sab_u.buf_y, sea_y + pos_w, red_w);
```
copies `red_w` (=4096) bytes from `sea_y + 0xFFFFFFFF`, which points ~4 GiB past the atom buffer. ASan-absent, this is a raw segfault; ASan-present, it's a heap-buffer-overflow read.

## Why the bound can be bypassed

`p_octs_w` is the declared octs length (attacker can set to e.g. 4096), and `pos_w` is the seek position (attacker can set to 2³²−1). The arithmetic at line 567 is performed in the narrower of the two types (32-bit) before comparison with `p_octs_w`. The correct check is either:

- `pos_w > p_octs_w || n_w > p_octs_w - pos_w` (rearranged to avoid overflow), or
- cast to wider type: `(uint64_t)pos_w + n_w > p_octs_w`.

## Reachability

Bytestream is a Hoon jet — invoked by `+bytestream:stream` in lib/stream or similar. Any Hoon code the peer supplies (Arvo %fact events, %peek scries, JSON pokes, etc.) can end up routing data through these jets. So this is a **remote-reachable** crash: a peer can craft a bytestream request with `pos=0xFFFFFFFF` that our runtime will process.

Worst case is not just a crash — `sea_y + pos_w` lands somewhere in process memory; the 4 KB copied into `sab_u.buf_y` is then returned as part of a Hoon noun (the `read_octs` result tuple). If the landing zone contains sensitive data (keys, secrets, heap metadata), the peer learns 4 KB of our address space per request.

**This is a potential info-leak, not just DoS.**

## Fix

Two correct options:

```c
/* option A: explicit wide check */
if ((uint64_t)pos_w + (uint64_t)n_w > (uint64_t)p_octs_w) {
  u3m_bail(c3__exit);
}
```

```c
/* option B: rearrange to avoid overflow */
if (pos_w > p_octs_w || n_w > p_octs_w - pos_w) {
  u3m_bail(c3__exit);
}
```

Apply the same fix at line 575 (`pos_w + n_w > len_w`).

Search the rest of `bytestream.c` for similar `pos_w + n_w`-style expressions; the pattern likely repeats.

## Reproducing

```bash
./fuzz/build.sh fuzz_bytestream
./fuzz/out/fuzz_bytestream.afl < fuzz/findings/014-bytestream_read_octs_overflow/repro.bin
```

Expected without ASan: SIGSEGV in `memcpy` via `_qe_bytestream_read_octs:596`.
With ASan: `heap-buffer-overflow READ of size 4096`.

## Additional variants

After ~20h of fuzzing, AFL produced two more crashes with the same root
cause but different `(pos_w, n_w)` combinations:

- `repro_variant_001.bin` — `n=3, pos=2148529163`. SIGSEGV in memcpy
  with 3-byte read well past the atom buffer.
- `repro_variant_002.bin` — larger input that trips ASan's
  `unknown-crash READ of size 4096 at 0x8000ffffc5d8` (wild pointer
  past the allocator arena). Same overflowing-arithmetic path.

All three reproducers resolve with the same fix (widen the `pos_w + n_w`
comparison to 64-bit). Retain all three in the repro set to exercise
the fix's boundary conditions.
