# Finding 017 — float division C-vs-Hoon systematic divergence

**Status:** open; needs reference comparison against the Hoon float
stdlib before classifying.
**Severity:** likely low-to-medium. Any Hoon program that does
`div:rs`, `div:rd`, or `div:rh` on edge-case values will observe
different results depending on whether the jet is hot. No memory
corruption; just silent numerical divergence.

## What the oracle found

Harness `fuzz_jet_l5_float` produced 5 crashes in ~8 minutes, ALL
float-division mismatches — not arithmetic for add/sub/mul. The
pattern spans three precisions:

| Width | Jet | Repro file | Jet mug | Hoon mug |
|---|---|---|---|---|
| 16b  | `div:rh` | `repro_rh.bin` | `4ce17f7a` | `5189b3d5` |
| 32b  | `div:rs` | `repro_rs.bin` | `64692041` | `7f78592a` |
| 64b  | `div:rd` | `repro_rd.bin` | `7916aa3c` / `2a491a2` | `7b09aa4b` / `5cc2a3dc` |

## Suspected root causes

Since add/sub/mul never diverged in the same soak, the problem is
specific to division. Most likely candidates:

1. **Rounding-mode drift** — softfloat (C side) vs. Hoon's
   interpretation may default to different IEEE rounding modes
   (RN vs. RZ vs. RD vs. RU). Division is the op most sensitive
   to rounding mode — the last bit of the mantissa is often at
   the boundary.
2. **Zero / NaN / Inf / denormal handling** — the first repro input
   (`f9ff ff00 dd7f 00 00 00 00`) decodes to `a = 0xDD00FFFF` and
   `b = 0x000000FF` as single-precision floats. `b` in particular
   is a very-small subnormal; `a/b` involves flushing and
   normalization that could diverge.
3. **Sign-of-zero** on `x/-0` — Hoon may preserve signed zero
   differently from the softfloat jet.

## Next steps

1. Decode each repro's `(a, b)` atoms into IEEE-754 floats,
   print `a / b` per C softfloat and compare to what `++div:rs`
   in hoon.hoon would produce step-by-step.
2. Check whether `a` or `b` in any repro is NaN, ±inf, ±0, or
   subnormal — those are the usual suspects.
3. Look at `pkg/noun/jets/e/rs.c` (the jet) and the Hoon stdlib's
   `++div:rs` body — diff the rounding setup.

## Impact assessment

Hoon code that does float arithmetic on regular non-edge values
is unaffected — the jet and Hoon agree on almost all inputs
(the 5 mismatches were found via havoc mutations, not routine
fuzzing). But ANY C-vs-Hoon divergence is a correctness issue
by Urbit's jet contract — the jet must match Hoon exactly.

## Reproducing

```bash
./fuzz/build.sh fuzz_jet_l5_float
./fuzz/out/fuzz_jet_l5_float.afl < fuzz/findings/017-float_div_c_vs_hoon_mismatch/repro_rs.bin
```

Expected: `test: div $: mismatch: good <X> bad <Y>` then harness
aborts with `jet_l5_float: ORACLE MISMATCH in 'div:rs'`.

## Related

- Finding #016 is also an adversarial-only oracle mismatch in the
  jet system (rub), but affects a much tighter attack surface.
  Both findings strengthen confidence that the ice-oracle fuzzer
  is catching real C-vs-Hoon drift.
