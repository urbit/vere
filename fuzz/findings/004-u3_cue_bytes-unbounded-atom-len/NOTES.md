# Finding 004 — `u3s_cue_bytes` accepts unbounded atom length from input

**Status:** FIXED. Bounds check landed in `pkg/noun/serial.c`
`_cs_cue_bytes_next` AND `_cs_cue_xeno_next` (the off-loom variant
used by ames/mesa via `u3s_cue_xeno`). Reproducer
`fuzz/regression/fuzz_u3_cue_bytes/004-unbounded-atom-len.bin` now
returns `bail: meme` cleanly.

**Severity:** denial of service via remote / crafted bytes. **High** —
this one IS reachable via Vere's production ingest paths
(`u3s_cue_bytes` is called from `_conn_moor_poke`, ames/mesa packet
handlers via `u3s_cue_xeno`, `u3_disk_sift` for event log replay).

## Trigger

5-byte crafted input:

```
00000000: 0000 0040 43                             ...@C
```

Reproduces:

```bash
./fuzz/build.sh fuzz_u3_cue_bytes
./fuzz/out/fuzz_u3_cue_bytes.afl < fuzz/findings/004-u3_cue_bytes-unbounded-atom-len/repro.bin
```

## Root cause

`_cs_cue_bytes_next` at `pkg/noun/serial.c:792-809`:

```c
case ur_jam_atom: {
  u3_atom vat;

  _cs_cue_need(ur_bsr_rub_len(red_u, &len_d));

  if ( 31 >= len_d ) {
    vat = (u3_noun)ur_bsr32_any(red_u, len_d);
  }
  else {
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, 0, len_d);

    ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
    vat = u3i_slab_mint_bytes(&sab_u);
  }

  return _cs_cue_put(har_p, bit_d, vat);
}
```

`len_d` (the claimed atom bit length) comes directly from the input
stream via `ur_bsr_rub_len`. There's no check that `len_d` fits within
the remaining bitstream — a 5-byte input can claim a 256 MB atom,
which `u3i_slab_init` then attempts to allocate on the loom.

This is the exact same bug as finding #001 in pkg/ur, just in u3's
parallel cue implementation.

## Crash mode

With the libsigsegv handler gated by `-DU3_FUZZ`, the allocation goes
through `u3a_walloc` → `_imalloc` → `_alloc_pages` → `_extend_heap`,
which is supposed to bail with `c3__meme` when it would collide with
the road's `cap_p`. In practice the bail check at
`pkg/noun/palloc.c:213-222` doesn't fire on these inputs, and execution
proceeds to `_alloc_pages:306` (`dir_u[pag_w] = u3a_head_pg`) which
segfaults on an out-of-loom directory pointer.

Whether normal urbit (with libsigsegv installed) catches this via
guard-page handling is a separate question and worth investigating.
The key issue is that **the input validation gap is at the cue layer,
not the allocator layer** — even if palloc handled the OOM gracefully,
a remote peer would still be able to trigger 256 MB allocation
attempts on demand, which is a clean DoS even without crashing.

## Sample stack trace (from gdb)

```
#0 _alloc_pages (siz_w=2049)         pkg/noun/palloc.c:306
#1 _imalloc (len_w=2049)              pkg/noun/palloc.c:541
#2 u3a_walloc (len_w=8388616)         pkg/noun/allocate.c:237
#3 _ci_slab_init (len_w=8388613)      pkg/noun/imprison.c:52
#4 u3i_slab_bare (len_d=268435590)    pkg/noun/imprison.c:153
#5 u3i_slab_init (len_d=268435590)    pkg/noun/imprison.c:127
#6 _cs_cue_bytes_next                 pkg/noun/serial.c:802
#7 u3s_cue_bytes (len_d=5)            pkg/noun/serial.c:845
```

The 5-byte input asks for an atom of 268,435,590 bits ≈ 32 MB.

## Production impact

Every cue entry point in vere is affected:

- `u3s_cue_bytes` — direct callers
- `u3s_cue_xeno` — used by `_conn_moor_poke` (`pkg/vere/io/conn.c`),
  ames packet bodies, mesa packet bodies
- `_cs_cue_xeno` — `u3s_cue_atom`, `u3s_cue_xeno_with`
- `u3ke_cue` — used by `u3_disk_sift` (event log replay)

A remote peer can send a few crafted bytes through any of these
channels to trigger huge allocations on a target ship. At minimum this
is a clean remote DoS (memory exhaustion); at worst, depending on
allocator state, it could crash the ship or trigger other downstream
issues.

## Suggested fix

Mirror the pkg/ur fix (finding #001):

```c
case ur_jam_atom: {
  u3_atom vat;

  _cs_cue_need(ur_bsr_rub_len(red_u, &len_d));

  // reject atoms that claim more bits than the bitstream has left;
  // catches crafted small inputs that demand huge allocations.
  if ( len_d > ((red_u->left << 3) - red_u->bits) ) {
    return u3m_bail(c3__meme);
  }

  if ( 31 >= len_d ) {
    ...
```

Verify the field names against `ur_bsr_t` in `pkg/ur/bitstream.h`
before landing — pkg/ur calls the bit cursor `bits` and the byte
counter `left`.

## Other crashes from H4

The first 30s fuzz burst found **12 crashes**, all in this same call
chain. Sample atom sizes from the gdb traces:
- 256 MB (smallest)
- 2 GB
- 8 GB

All variations of the same root cause. Once the bounds check lands,
these all become `c3__meme` rejections instead of segfaults.
