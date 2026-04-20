# Finding 002 — `ur_cue` produces non-canonical iatoms, violating hashcons-uniqueness invariant

**Status:** filed as a latent correctness bug, **not** fixed in the
source tree (per user direction). Worked around in the H3 harness by
comparing nouns structurally instead of by nref identity.

**Severity:** low. Not reachable via any Vere production ingest path.
Affects pkg/ur's internal contract and any direct consumer of
`ur_cue` (currently only `pkg/vere/benchmarks.c` and pkg/ur's own
tests).

## Invariant at stake

`pkg/ur/hashcons.h:79-82`:

> cells are hash-consed, atoms are deduplicated (byte-array
> comparison), mug hashes are stored, and **noun references are
> unique within a root.**

That uniqueness is the basis for pkg/ur's entire equality model:
two `ur_nref`s represent equal nouns iff they compare bit-equal as
`uint64_t`. pkg/ur ships no structural-equality primitive.

## Violation

`_cue_next` at `pkg/ur/serial.c:234-258`:

```c
case ur_jam_atom: {
  ...
  else if ( 62 >= len ) {
    *out = (ur_nref)ur_bsr64_any(bsr, len);   // direct atom path
  }
  else {
    uint64_t len_byt = (len >> 3) + !!ur_mask_3(len);
    uint8_t  *byt    = _oom("cue_next bytes", calloc(len_byt, 1));

    ur_bsr_bytes_any(bsr, len, byt);

    //  strip trailing zeroes
    while ( len_byt && !byt[len_byt - 1] ) {
      len_byt--;
    }

    *out = ur_coin_bytes_unsafe(r, len_byt, byt);
  }
}
```

The direct-vs-indirect branch is on the **claimed** atom bit-length
(`len`), not on the actual value size after trailing-zero trim. A
crafted input can claim `len = 63` (forcing the indirect path) while
the actual content, after trimming, fits in a 62-bit direct atom.

Cue then calls `ur_coin_bytes_unsafe`, which inserts the iatom into
the hashcons dict without re-checking for direct-atom promotion.
`ur_coin_bytes` (the safe variant at `hashcons.c:663`) does do this
check:

```c
ur_nref
ur_coin_bytes(ur_root_t *r, uint64_t len, uint8_t *byt)
{
  while ( len && !byt[len - 1] ) len--;
  if ( 62 >= ur_met0_bytes_unsafe(len, byt) ) {
    // produce a direct atom
  } else {
    // copy + call unsafe
  }
}
```

so the canonicalisation logic already exists in pkg/ur — cue just
doesn't use it.

## Reproducer

`classA.bin` (11 bytes, from AFL++ run 2026-04-14):

```
00000000: 80ff 0505 050d 0505 0505 05              ...........
```

Invoking `ur_cue` on this input and then jam + re-cue:

```
ref1 = 0x4000000000000000   (ur_iatom, idx 0)
ref2 = 0x282828286828282f   (ur_direct, value 0x282828286828282f)
```

Both represent atom value `0x282828286828282f`, but cue produced an
iatom the first time (because input claimed `len > 62`) and a direct
atom the second time (because jam emitted the minimal encoding and
re-cue took the direct path).

`classB.bin` (34 bytes) shows the downstream effect: because a cell
containing a non-canonical iatom has a different hashcons key than
the same cell containing the canonical direct atom, the "same"
cell ends up stored twice in the root's cells table — two distinct
`ur_icell` indices for structurally equal cells.

## Blast radius

Callers of `ur_cue` that grep turns up outside pkg/ur itself:

```
pkg/vere/benchmarks.c:97, 292, 319
```

`pkg/noun/serial.c` has its own cue (`_cs_cue_bytes_next`) that
bypasses `ur_cue` entirely. It builds `u3_noun`s via
`u3i_slab_mint_bytes`, which canonicalises via `u3i_word` for
single-word atoms and trims trailing zero words before
`_ci_atom_mint`. So Vere's real ingest paths —
`u3s_cue_bytes`, `u3s_cue_xeno`, `u3_disk_sift`, ames, mesa, newt —
are **unaffected**.

The bug is therefore reachable only by benchmarks and any research
code that calls `ur_cue` directly. No remote attack surface.

## Suggested fix (not landed)

Mirror the `ur_coin_bytes` logic inside `_cue_next`:

```c
//  strip trailing zeroes
while ( len_byt && !byt[len_byt - 1] ) {
  len_byt--;
}

//  promote to direct atom if the stripped value fits in 62 bits
if ( 62 >= ur_met0_bytes_unsafe(len_byt, byt) ) {
  uint64_t i, direct = 0;
  for ( i = 0; i < len_byt; i++ ) {
    direct |= (uint64_t)byt[i] << (8 * i);
  }
  free(byt);
  *out = (ur_nref)direct;
}
else {
  *out = ur_coin_bytes_unsafe(r, len_byt, byt);
}
```

~10 lines. Restores the hashcons-uniqueness invariant.

## Related

- `ur_bytes` in `pkg/ur/hashcons.c:524` has a separate use-after-scope
  hazard for direct atoms: it returns `*byt = (uint8_t*)&ref` where
  `ref` is the parameter stack slot. Callers that read through `*byt`
  after the function returns are invoking UB. The H3 harness
  deliberately avoids `ur_bytes` and does its own byte extraction to
  sidestep this.

## Workaround in the fuzz harness

H3 (`fuzz/harnesses/fuzz_ur_jam_cue_diff.c`) no longer relies on nref
identity. It walks both noun trees in lockstep, comparing cells
recursively and atoms by byte representation (handling all four
direct/iatom crossings uniformly). If the canonicalisation bug is
fixed later, the harness continues to work — the structural walker
just gets faster because the fast-path `a == b` hits more often.
