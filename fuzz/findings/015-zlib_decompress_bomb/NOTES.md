# Finding 015 — `u3qe_decompress_zlib` decompression bomb (unbounded slab grow)

**Status:** real bug, not yet patched.
**Severity:** remote DoS — loom exhaustion → segfault.
**Reachability:** any Hoon peer code that can call `+decompress-zlib:stream`
(or the gzip variant, same pattern at line 181+) with attacker-controlled
compressed bytes — routine in `lib/stream`, HTTP decoding, archive import,
Arvo event bodies.

## The bug

`pkg/noun/jets/e/zlib.c:108-157` decompress loop has NO upper bound on
`strm.total_out`. It keeps growing the output slab until either zlib
returns `Z_STREAM_END`, or the loom allocator fails mid-realloc and
segfaults.

```c
while ((ret = inflate(&strm, Z_FINISH)) == Z_BUF_ERROR) {
  if (strm.avail_out == 0) {
    strm.avail_out = chunk_w;
    u3i_slab_grow(&sab_u, 3, strm.total_out + chunk_w);  // unbounded
    strm.next_out = sab_u.buf_y + strm.total_out;
  }
  ...
}
```

## Trigger

54 KB of `\x97`-repetition deflate stream. Deflates trivially; inflates
to enough output to exhaust the 64 MiB loom. Crash lands in
`_irealloc` → `memcpy` at `pkg/noun/palloc.c:980`.

Minimal repro is in `repro.bin`. Amplification ratio: ~1000×.

## Call chain

```
_qe_decompress_zlib (jet impl)
  → _decompress (zlib.c:89)
    → u3i_slab_grow (unbounded)
      → u3a_wealloc
        → _irealloc → SIGSEGV
```

## Fix

Cap `strm.total_out` at a sane ceiling and bail if exceeded. Example:

```c
#define MAX_INFLATE (1u << 24)   /* 16 MiB — well above any legit use */

if (strm.total_out + chunk_w > MAX_INFLATE) {
  u3l_log("decompress: output exceeds %u bytes", MAX_INFLATE);
  inflateEnd(&strm);
  u3i_slab_free(&sab_u);
  return u3m_bail(c3__exit);
}
u3i_slab_grow(&sab_u, 3, strm.total_out + chunk_w);
```

Audit the gzip variant at `zlib.c:181+` — same loop pattern, same fix
likely needed. Also `+decompress-gzip` jet.

## Reproducing

```bash
./fuzz/build.sh fuzz_zlib_de
./fuzz/out/fuzz_zlib_de.afl < fuzz/findings/015-zlib_decompress_bomb/repro.bin
```

Expected: SIGSEGV in `_irealloc` via `u3i_slab_grow` call chain.
