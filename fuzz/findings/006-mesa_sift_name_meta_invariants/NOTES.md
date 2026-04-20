# Finding 006 — `_mesa_sift_name`: assertion failures on conflicting metadata bit-fields

**Status:** **FIXED locally** in `pkg/vere/io/mesa/pact.c` by
replacing the asserts with a `_sift_fail` + early return. Both
reproducers (`repro_tau.bin`, `repro_gaf.bin`) now exit cleanly and
are in `fuzz/regression/fuzz_mesa_sift_pact/`. Push back to
upstream as a follow-up to PR 998.

Originally discovered after applying urbit/vere PR 998 to fix
finding #005. PR 998's `_sift_next` reordering and `_mesa_sift_name`
buffer-management cleanup blocks the original repro but exposes
this new class of asserts in the same function.

**Severity:** assertion abort triggered by crafted bytes. Reachable
from any mesa packet ingest path — same surface as #005.

## Trigger

Two minimal reproducers, one per assertion:

`repro_tau.bin` (7 bytes) — fires `assert(!met_u.tau_y)`:

```
00000000: fafa 01fa fafa 80
```

`repro_gaf.bin` (23 bytes) — fires `assert(!met_u.gaf_y)`:

```
00000000: 00ef 0d9a 9a9a 9a9a 9a9a 9a9a 9a9a 9a9a
00000010: 9a9a 9a9a 9a9a 00
```

Both reproduce against the urbit/vere PR 998 branch:

```bash
git apply /path/to/pr998.diff
./fuzz/build.sh fuzz_mesa_sift_pact
./fuzz/out/fuzz_mesa_sift_pact.afl < repro_tau.bin
./fuzz/out/fuzz_mesa_sift_pact.afl < repro_gaf.bin
```

## Root cause

`_mesa_sift_name` at `pkg/vere/io/mesa/pact.c:634-669`:

```c
u3_mesa_name_meta met_u = {0};
met_u.ran_y = _sift_bits(sif_u, 2);
met_u.rif_y = _sift_bits(sif_u, 2);
met_u.nit_y = _sift_bits(sif_u, 1);    // "init" flag
met_u.tau_y = _sift_bits(sif_u, 1);    // "tau" flag (auth)
met_u.gaf_y = _sift_bits(sif_u, 2);    // "gaf" field
...
if ( met_u.nit_y ) {
  assert( !met_u.tau_y );    // line 655
  assert( !met_u.gaf_y );    // line 656
  // XX init packet
  nam_u->fra_d = 0;
}
```

Each of `nit_y`, `tau_y`, and `gaf_y` is read directly from the input
bitstream. The parser believes these bit-field combinations are
mutually exclusive (an init packet shouldn't also be auth'd, and
shouldn't have a gaf field) and asserts the invariant. But nothing
in the wire format actually enforces mutual exclusion — a crafted
input can set all three bits at once.

The TODO comment `// XX init packet` suggests this code path was
intended to be filled in but wasn't, leaving the asserts as
placeholders.

## Stack trace

```
_mesa_sift_name        pkg/vere/io/mesa/pact.c:655 (or :656)
_mesa_sift_poke_pact   pkg/vere/io/mesa/pact.c:838
_mesa_sift_pact        pkg/vere/io/mesa/pact.c:890
mesa_sift_pact_from_buf pkg/vere/io/mesa/pact.c:940
```

## Production impact

Same as #005: every mesa UDP receive path. A 7-byte crafted packet
can crash any vere instance running mesa.

## Suggested fix

Convert the asserts to `_sift_fail` calls so malformed input gets a
graceful parse error instead of an abort:

```c
if ( met_u.nit_y ) {
  if ( met_u.tau_y || met_u.gaf_y ) {
    _sift_fail(sif_u, "init packet has tau or gaf");
    return;
  }
  nam_u->fra_d = 0;
}
```

This preserves the invariant intent (init packets shouldn't have
those fields) while degrading gracefully on malformed input.

## Other observations from this fuzz run

- 8 crashes total in 60s, all in `_mesa_sift_name` lines 655 and 656
  (different combinations of input bytes triggering each path).
- One zero-output crash on a 3-byte input — different class, may be
  worth a separate finding.

## Reproducing

PR 998 must be applied for this finding to surface (otherwise the
older #005 assert fires first).
