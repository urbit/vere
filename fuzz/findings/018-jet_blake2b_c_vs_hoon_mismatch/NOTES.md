# Finding 018 — `++blake2b` Hoon bug in `get-word-list`

**Status:** root-caused. This is a real correctness bug in the Hoon
`++blake2b` at `sys/zuse.hoon:2557-2736`. The C jet is RFC 7693-
compliant (matches `urcrypt_blake2` and Python `hashlib.blake2b`).
The Hoon diverges whenever a padded 128-byte blake2b block atom's
natural bit-length is less than 1024 bits — i.e., when the block's
highest 64-bit word is zero.

**Severity:** medium-high correctness bug. Affects every caller of
`blake2b:blake:crypto` whose message (or key) layout produces a
sparse padded block. Hidden by the existing test-vector suite at
`tests/sys/zuse/crypto/blake.hoon` because those vectors use dense
non-zero message bytes.

## Root cause

`++get-word-list` in the blake2b core at `sys/zuse.hoon:2588-2594`:

```hoon
++  get-word-list
  |=  [h=@ w=@ud]
  ^-  (list @)
  %-  flop
  =+  l=(rip 6 h)
  =-  (weld - l)
  (reap (sub w (lent l)) 0)
```

This prepends `(w - lent l)` zeros to the rip-6 list `l`, then
flops. Result: `[word(N-1), ..., word(0), 0, 0, ...]` — the zero
padding is at the BOTTOM (low-word) end of the flopped list.

For a 128-byte block atom whose bit-length is less than 1024 bits,
`rip 6 h` returns fewer than 16 words. The zero-padding should
land at the TOP of the flopped list (the high-word positions, which
are currently zero in the original atom because its met is short),
but `weld - l` puts the zeros at the BOTTOM (low-word positions).

Effect: `get-word c 0 16` — which the compressor uses as m[0] —
returns the highest non-zero 64-bit word of `c`, not the highest
word of the 128-byte block view. Every subsequent word index is
shifted down by the number of missing top-zero words, leaving one
(or more) spurious zeros at the bottom that the compressor reads
as m[N..15].

## Correct form

```hoon
++  get-word-list
  |=  [h=@ w=@ud]
  ^-  (list @)
  =+  l=(rip 6 h)
  %-  flop
  (weld l (reap (sub w (lent l)) 0))
```

Append zeros at the END of the rip list (high-word side, pre-flop),
so the flopped list has zeros at the TOP and data at the bottom.

Equivalent fix would update `put-word` in the same core — which
relies on the same layout — or make both compute on padded lists
consistently.

## Threshold

Trigger condition: the padded 128-byte block atom has `met 3` < 128
bytes. Concretely, the highest 64 bits of the block are zero.

For a simple case `msg = <n-2 zero natural bytes> | 0xed 0x09` with
no key and `out=10`:

| natural msg bytes | padded met | rip words | pad | match/mismatch |
|---|---|---|---|---|
| 3    | 127 | 16 | 0 | ok |
| 5    | 125 | 16 | 0 | ok |
| 7    | 123 | 16 | 0 | ok |
| 8    | 122 | 16 | 0 | ok |
| 9    | 121 | 16 | 0 | ok |
| **10** | 120 | 15 | 1 | **MISMATCH** |
| 11   | 119 | 15 | 1 | MISMATCH |
| 12   | 118 | 15 | 1 | MISMATCH |
| 17   | 113 | 15 | 1 | MISMATCH |
| 19   | 111 | 14 | 2 | MISMATCH |

The transition at 10 bytes is exactly the point where the padded
block atom transitions from "met > 15 words" to "met ≤ 15 words"
for a 2-byte-met msg atom. The general condition is
`met(msg_atom_after_end) + (128 - wid) < 128` — i.e., whenever
`met(msg_atom) < wid`.

## Historical note

`tests/sys/zuse/crypto/blake.hoon` (added 2018-11-06 by Fang) covers
three test vectors, all with dense message bytes (byte sequences
`01..21`, `01..ff`, etc.). None of these produce sparse padded
blocks, so the `get-word-list` bug is never exercised by the tests.

## Three independent confirmations of the C jet

For repro1 (`msg = 0x0*17, ed, 09`, `key = f7 f7 b6 d9 04 3f, 0*13`,
`out = 10`, `wid = wik = 19`):

- C jet `u3we_blake2b`:       `f353da7ff766903011d0`
- `urcrypt_blake2` direct:    `f353da7ff766903011d0`
- Python `hashlib.blake2b`:   `f353da7ff766903011d0`
- Hoon `++blake2b`:           `8df8b9b927b5671989a9`  ← wrong

## Impact

- Any Hoon code hashing sparse messages (leading/trailing zero bytes,
  or payloads short of their declared `wid`) gets incorrect hashes.
- Interop with non-Urbit blake2b implementations (Ethereum,
  libsodium, RFC 7693 reference) is broken for sparse inputs.
- Jetted execution hides the bug in production because `_cj_kick_z`
  defaults to trusting the C jet (`ice = c3y`). Only un-jetted paths
  — or any future chain of trust that eventually goes back to the
  Hoon source — see the wrong value.
- If a consumer ever runs without jets (bootstrap, fallback path, or
  signature verification against a precomputed hash of Hoon output)
  the divergence will surface.

## Fix scope

One-line change in `sys/zuse.hoon` inside the blake2b chapter — swap
`(weld - l)` for `(weld l -)` and move flop to outside, or rewrite
the helper per the "correct form" section above. No C changes
needed; the jet is correct.

## Reproducing

```bash
# Requires the brass-pill pier at /tmp/fuzz-pier-zod-v44.
./fuzz/build.sh blake2b_probe
fuzz/out/blake2b_probe.afl fuzz/findings/018-jet_blake2b_c_vs_hoon_mismatch/repro1.bin
# or any sparse message:
fuzz/out/blake2b_probe.afl --raw 0000000000000000000000000000000000ed09 "" 10
```

Both print:

```
test: blake2b $: mismatch: good <hoon_mug> bad <jet_mug>
  good (hoon) 10 bytes: <wrong bytes>
  bad  (jet)  10 bytes: <RFC-correct bytes>
```

## Current coverage

`fuzz_jet_l11_base` found two reproducers organically in ~6 h of
soak (brass pill, 29 exec/s, ~631k execs). The harness feeds
3-partitioned random bytes as `[msg=octs key=octs out=@ud]` which
produces sparse padded blocks with high probability.
