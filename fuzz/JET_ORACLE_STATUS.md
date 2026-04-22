# Jet ice-oracle fuzz campaign — status

**Mechanism:** vere's `u3j_harm.ice` flag (`pkg/noun/jets.h:52`) gates
a dormant differential-test path in `_cj_kick_z`
(`pkg/noun/jets.c:887-921`). When `ice=c3n`, the runtime runs both
the C jet and the Hoon formula for the same input, compares with
`u3r_sing`, and bails `%fail` on mismatch. Every jet ships with
`ice=c3y` by default — the mechanism was never active. We added:

- `u3j_fuzz_arm(const c3_c*)` — walks `u3D.dev_u` and flips
  `ice=c3n` on named cores at runtime.
- `u3j_Fuzz_testing` global — suppresses nested-oracle recursion
  during the Hoon side, so jet A's Hoon formula calling jet B
  doesn't re-run the oracle on B (otherwise O(n²) blowup).

Both gated by `#ifdef U3_FUZZ`.

## Harnesses (5, running concurrently)

| Harness | Layer | Jets | Status |
|---|---|---|---|
| `fuzz_jet_l1_math` | 1 (math) | add/dec/sub/mul/div/mod/gte/gth/lte/lth/max/min/cap/mas/peg/dvr | 18+ cycles, saturated, **clean** |
| `fuzz_jet_l2_bits` | 2 (bit ops) | mix/dis/con/mug/end/lsh/rsh/cut/rep/rip/rev/swp/met/cat/dor/gor/mor | 21+ cycles, saturated, **clean** |
| `fuzz_jet_l3_list` | 3 (lists) | flop/lent/snag/slag/scag/reap/welp | 19+ cycles, saturated, **clean** |
| `fuzz_jet_l4a_crypto` | 4a (hashes) | shax/shay/shal/mug | 12+ cycles, **clean** |
| `fuzz_jet_l4b_encode` | 4b (encoders) | fein:ob/fynd:ob/scot/scow/slaw/mat/rub | 1+ cycle, **1 finding** |

Harnesses share a structure:

1. Boot noun + ivory pill (needs vere's `u3v_boot_lite`).
2. Call `u3v_wish("<name>")` for each target jet to warm the cache
   BEFORE flipping ice. (Flipping first causes the wish itself to
   hit the oracle during compilation — 25s → 7min boot.)
3. Flip ice via `u3j_fuzz_arm` on each jet's core.
4. Fuzz-loop: pick a mode byte, build a sample of the right shape,
   slam via `u3n_slam_on`. On `c3__fail` bail, `abort()` — AFL
   records as crash; harness prints `ORACLE MISMATCH in '<jet>'`.

## Performance characteristics

- Oracle mode runs 10-100× slower than pure-C jet fuzz — each
  iteration does jet + hoon + compare.
- With nested-test short-circuit: ~50 exec/s per L1 harness,
  ~70-200 exec/s for higher layers.
- **Critical tuning:** atoms get clamped per jet based on the
  slowest recursive path in the Hoon formula. e.g. dec atoms
  capped at 2^16 (65536) because Hoon `++dec` is O(a). See
  `fuzz_jet_l1_math.c` clamp table.

## Findings

### #016 — `rub` C-vs-Hoon divergence (adversarial-only)

**Location:** `pkg/noun/jets/e/rub.c:48-61`
**Trigger:** 7-byte input `06 00 00 00 00 00 c0` causes the oracle
to compute different `e` (and thus different `p`, `q`).

Root cause (tentative): C reads payload bits `[a+c+1, a+2c)`, Hoon
reads `[a+c, a+2c-1)` — off-by-one in the bit range.

**Impact: low.** Verified via `fuzz/harnesses/rub_roundtrip.c`:
`rub(0, mat(x)) == (bitlen, x)` for all x in `[0, 4096)`. The C jet
is self-consistent on real jam-encoded inputs; the divergence only
fires on bit patterns that can't be emitted by `mat`.

Full write-up: `fuzz/findings/016-jet_rub_c_vs_hoon_mismatch/`

## Self-test

The oracle pipeline was verified end-to-end by injecting a
deliberate bug into `u3qa_dec` (return `a-2` for a≥100 instead of
`a-1`). The harness correctly detected it with:

```
test: dec $: mismatch: good 28e02a73, bad 22e98abd
jet_l1_math: ORACLE MISMATCH in 'dec'
```

After confirming, the injection was reverted. Proof that the
oracle fires on real C-vs-Hoon drift, not just harness artifacts.

## Next possible extensions (not yet in scope)

- Layer 5+ : parser combinators (`pfix`/`plug`/`pose`/`bend`/etc),
  type-system jets (`ut_*`), WASM (`urwasm`). All need structured
  noun seeds — random bytes won't produce valid inputs.
- Wider crypto coverage: ripe, crc32, adler32, sha1, blake2b,
  argon2 are not in the ivory pill; would need a fuller pill.
- Differential of `cue` vs hand-written decoder: bypass the
  re-entrancy problem by comparing `u3qe_cue` against `ur_cue`
  directly (without going through `_cj_kick_z`).
