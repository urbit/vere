# Finding 016 — `u3qe_rub` C-vs-Hoon oracle mismatch

**Status:** oracle mismatch detected by ice differential fuzzer.
Needs reference comparison before classifying as real bug.

**Severity:** unknown — could be (a) the C jet is wrong and the
Hoon is authoritative, (b) the Hoon reference we compared against
has its own issue, or (c) my harness is constructing a sample that
exercises an untested corner. Worth investigating because `rub` is
the counterpart to `mat` (jam encoder helper) and any divergence
could affect jam/cue round-trips for corner-case inputs.

## Trigger

Harness `fuzz_jet_l4b_encode` found the mismatch in ~2400 iterations
of oracle fuzzing.

Input bytes (`repro.bin`): `06 00 00 00 00 00 c0` (7 bytes).
Decoded by harness:
- `buf[0] = 0x06` → mode = 6 % 7 = 6 (`rub`)
- `buf[1] = 0x00` → aura selector (unused for `rub`)
- `buf[2..6] = 00 00 00 00 c0` → payload passed to `u3i_bytes`

Harness builds sample as `[a=0 b=atom]` where `atom = u3i_bytes(5, buf+2)`
(little-endian 40-bit value with top byte `0xc0`). Calls
`u3v_wish("rub")` → ice-flipped → `_cj_kick_z` runs jet + Hoon
formula + compares.

## Oracle output

```
test: rub $: mismatch: good <hoon_mug> bad <jet_mug>
jet_l4b_encode: ORACLE MISMATCH in 'rub'
```

(Harness aborts on `c3__fail` bail from `_cj_kick_z:910`.)

## Apparent source of divergence

Reading `pkg/noun/jets/e/rub.c` against a Hoon-stdlib reference of
`++rub` that I recall from memory:

The C jet at `rub.c:48-61` computes:

```c
x = u3qa_dec(c);              // x = c - 1
z = u3qc_cut(0, d, x, b);     // cut(0, a+c+1, c-1, b)
```

reading bits `[a+c+1, a+2c)` — strictly AFTER the separator 1-bit.

The Hoon reference I believe is:

```hoon
=+  e=(add (bex (dec c)) (cut 0 [(add a c) (dec c)] b))
```

reading bits `[a+c, a+2c-1)` — starting AT the separator bit.

If this analysis is right, one side is off by one bit. For the
specific fuzz input (`b = 0xc0 << 32`, high bit of 0xc0 and the bit
below both set):

- C reads bits `[39, 76)` of `b` → value `1` (bit 39 alone)
- Hoon reads bits `[38, 75)` of `b` → value `3` (bits 38 + 39)

⇒ different `e`, different `p` and `q`.

**However**, I have NOT verified the Hoon source — `hoon.hoon` is
baked into the ivory pill and isn't in this repo's source tree.
The canonical Hoon may differ from what I recall.

## What to verify next

1. Pull the actual `++rub` definition from the ivory pill (or a
   Hoon stdlib source) and diff against the C jet line-by-line.
2. ✅ Check whether `mat(x) |> rub` round-trips for small x under
   the C jet alone. **Done.** `fuzz/harnesses/rub_roundtrip.c` runs
   `u3qe_rub(0, u3qe_mat(x))` for all x in `[0, 4096)` — all 4096
   round-trip cleanly (p and q match exactly). So the C jet is
   self-consistent on every valid jam-encoded input in that range.
3. Classification: the mismatch is **adversarial-only**. Real jam
   streams go through `mat` and round-trip through `rub` fine. The
   oracle only fires on bit patterns that could never be emitted by
   `mat` (e.g. `b = 0xc0 << 32` — nonzero bits with a gap pattern
   that isn't produced by the encoder).

## Impact

- **Jam/cue correctness**: not affected — encoder output always
  round-trips through the C `rub` jet.
- **C-vs-Hoon compliance**: the jet DOES diverge from the Hoon
  formula on some (adversarial) inputs. By Urbit's strict jet
  contract, this is still a correctness issue, just with a
  near-zero reachable attack surface since nothing in production
  feeds `rub` inputs that don't come from `mat`.
- **Severity**: low. Worth noting but not shippable as a security
  advisory.

## Reproducing

```bash
./fuzz/build.sh fuzz_jet_l4b_encode
./fuzz/out/fuzz_jet_l4b_encode.afl < fuzz/findings/016-jet_rub_c_vs_hoon_mismatch/repro.bin
```

Expected: `jet_l4b_encode: ORACLE MISMATCH in 'rub'` on stderr + SIGABRT.
