# Finding 009 — `_sift_var_word`: assertion failure on `len_w > 4`

**Status:** **FIXED locally** in `pkg/vere/io/mesa/pact.c`:
`_sift_var_word` now calls `_sift_fail` and returns 0 when
`len_w > 4` instead of asserting. Reproducer in
`fuzz/regression/fuzz_mesa_sift_pact/009-var_word.bin`. Push back
to upstream with #006.

Originally discovered after PR 998 was applied.

**Severity:** assertion abort. Same class as #005 / #006: crafted
mesa packet bytes drive the parser into an internal-state invariant
violation. Reachable from any mesa packet ingest path.

## Trigger

57-byte crafted input:

```
00000000: 92fe 1212 0002 e067 0064 0012 0112 1200
00000010: 02e0 67e0 6712 1212 1212 1212 1212 0ec1
00000020: fdff e902 0000 00ff 00cc 1212 12da 1212
00000030: 1212 122a 1212 0ec1 fd
```

Reproducing:

```bash
git apply pr998.diff   # if not already applied
./fuzz/build.sh fuzz_mesa_sift_pact
./fuzz/out/fuzz_mesa_sift_pact.afl < fuzz/findings/009-mesa_sift_var_word_assert/repro.bin
```

Output:

```
fuzz_mesa_sift_pact.afl: pkg/vere/io/mesa/pact.c:464:
  c3_w _sift_var_word(u3_sifter*, c3_w):
  Assertion `len_w <= 4` failed.
```

## Root cause

`_sift_var_word` at `pkg/vere/io/mesa/pact.c:461`:

```c
static c3_w
_sift_var_word(u3_sifter* sif_u, c3_w len_w)
{
  assert ( len_w <= 4 );        // line 464
  c3_y *res_y = _sift_next(sif_u, len_w);
  ...
}
```

The function asserts that the requested word length is ≤ 4 bytes
(matches `c3_w` width). But callers compute `len_w` from a bit field
in the packet, and the bit field can hold values larger than 4. A
crafted packet sets the bit field to `> 4` and the assertion fires.

This is the same shape as #005 / #006: the parser believes a
preceding bit field constrains the value, but nothing in the wire
format enforces that constraint.

## Production impact

`mesa_sift_pact_from_buf` is called from the UDP receive path. A
remote peer can send the 57-byte crafted packet to any vere
instance running mesa and crash it. No authentication required.

Severity is the same as #005/#006: remote DoS, no memory corruption.

## Suggested fix

Replace the assert with a `_sift_fail` so malformed input becomes
a graceful parse error:

```c
static c3_w
_sift_var_word(u3_sifter* sif_u, c3_w len_w)
{
  if ( len_w > 4 ) {
    _sift_fail(sif_u, "var-word length exceeds 4 bytes");
    return 0;
  }
  c3_y *res_y = _sift_next(sif_u, len_w);
  ...
}
```

## Related

- #005 fixed by PR 998
- #006 (sibling, in `_mesa_sift_name`) is open
- This is one more in the same class; suggest bundling all of #006,
  #009, and any others into a follow-up PR after PR 998 lands.

## Found by

H7 `fuzz_mesa_sift_pact` background fleet, ~4 minutes into the run
after PR 998 was applied to the working tree.
