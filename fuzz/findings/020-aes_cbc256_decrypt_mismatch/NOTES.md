# Finding 020 — AES-CBC decrypt (192 + 256) oracle mismatch

**Update 2026-04-23:** additional repro `repro_cbcb.bin` hits the same
bug in **AES-192 CBC** (`de:cbcb:aes:crypto`). Same symptom — C jet
returns 0 bytes, Hoon returns 16 bytes of decrypted-zero-ciphertext
output. Confirms the edge-case bug affects ALL three CBC key sizes
(128/192/256); likely `de:cbca` also breaks. Mug pair for cbcb:
`good 5efcf182, bad 79ff04e8` → Hoon `13460e87a8fc023ef2501afe7ff51c51`,
jet empty.

---

# Finding 020 — `de:cbcc:aes:crypto` (AES-256 CBC decrypt) oracle mismatch

**Status:** ORACLE MISMATCH detected by `fuzz_jet_l12_aes` in 41 min of
soak (615 iterations). C jet returned 0 bytes; Hoon returned 16 bytes.

**Severity:** medium-high. AES-256 CBC is used for symmetric encryption
in Urbit wire formats. Silent C-vs-Hoon divergence means any consumer
that ever runs jet-off (bootstrap, fallback, or Hoon-side reference
implementation comparison) gets incompatible ciphertexts.

## Trigger

Repro: 2 bytes `0b 00` — `buf[0]=0x0b → mode = 11 % 18 = 11 → de:cbcc:aes:crypto`.

Arm sample: `txt=@` (empty-atom from `0x00`). Because `cbcc` is a door
taking `[key=@I prv=@I]` and we wished `de:cbcc:aes:crypto` without
injecting a key, key=prv=0 defaults in. Oracle compares:

- **C jet result**: 0 bytes (jet returned empty atom or u3_none-like).
- **Hoon result**: 16 bytes = `67671ce1fa91ddeb0f8fbbb366b531b4`.

These are mugs `good 2ab4b625` vs `bad 79ff04e8` — wildly different.

## Hypothesis

The C jet likely short-circuits on `txt=0` (empty ciphertext) and
returns 0. The Hoon formula decrypts 0 as a single-block (16 bytes of
zero plaintext XOR'd with the AES-256 decryption of the 0-block under
key=0, prv=0) and produces a deterministic 16-byte output.

If correct, this is an edge-case divergence: real callers always
pass non-zero ciphertext, but per jet contract the jet must match
the Hoon on every input including `txt=0`.

## Reproducing

```bash
# brass pier at /tmp/fuzz-pier-zod-v44 must be present
./fuzz/build.sh fuzz_jet_l12_aes
./fuzz/out/fuzz_jet_l12_aes.afl < fuzz/findings/020-aes_cbc256_decrypt_mismatch/repro.bin
```

Expected: `test: de $: mismatch: good 2ab4b625, bad 79ff04e8` +
SIGABRT.

## Next steps

1. Read `pkg/noun/jets/138/aes.c` (u3we_cbcc_de) and check what it
   does for `txt=0`.
2. Diff against `sys/zuse.hoon:639-800` (cbcc core).
3. Classify: real bug vs adversarial-only (like #016).

## Caveats

- Door arms with default key=0 aren't a realistic attack surface by
  themselves, but the jet spec is strict: any input mismatch is a
  jet-contract violation.
- Worth also checking `de:cbca`, `de:cbcb` (128/192) for the same
  zero-ciphertext edge case.
