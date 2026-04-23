# Finding 023 — `make-k:secp256k1:secp:crypto` oracle mismatch

**Status:** ORACLE MISMATCH. Found by `fuzz_jet_l13_asym` in ~2h 16min
of soak (32k iterations). C jet and Hoon produce different 32-byte
nonces for the same (hash, private-key) input.

**Severity:** HIGH. **This is likely the root cause of finding #021**
(ecdsa-raw-sign mismatch). `make-k` is the deterministic nonce
function used by `ecdsa-raw-sign` — if the nonce differs, so does
the signature. Any wire protocol or user-facing signing flow that
goes through the Hoon reference (bootstrapping, pure-Hoon crypto
audits, or cross-implementation interop) produces incompatible
signatures from the C-jetted ones.

## Repro

47 input bytes (mode selector `0xff` → arm 0 = make-k):

```
ffff ffff ffff ffff ffff ffff 9c00 0000    hash[0..15]
ffff ffff ffff ffff ffff ffff ffff ffff    hash[16..31]
ffff ff7f ffff ffff ffff ffff ffff c021    priv[0..15]
6eb7 0203 04af 09c0 b309 50                priv[16..30] (short)
```

hash = `0xffffffffffffffffffffffff0000009cffffffffffffffffffffffffffffffff`
priv (used as 32 bytes, short-padded) starts `c021ffffffffff...7fffffff`.

Oracle output:

```
test: make $: mismatch: good 4ebdf2dc, bad 324f2046
  good (hoon) 32 bytes: 784723ce0438e0b78dbe90349b66961682cb8b02689c57cfa36a585f1a2c43bd
  bad  (jet)  32 bytes: 89e949f1aaf0d0aa2c0b5322b526f7a57e8f5676c35e3f627a92e060df2bf455
```

Both sides return 32-byte atoms (well-formed), but the bytes differ
completely — a classic nonce-derivation divergence (different PRF,
different seed, or different truncation rules).

## Relationship to #021

`make-k` is called internally by `ecdsa-raw-sign` (zuse.hoon:2254).
If `make-k` diverges, `ecdsa-raw-sign` inherits the divergence.
Fixing `make-k` likely closes #021 at the same time. Treat these as
a single root bug with two observation points.

## Hypothesis

The C jet is probably `urcrypt_secp_sign` → libsecp256k1's
`secp256k1_nonce_function_rfc6979`. The Hoon `make-k` in zuse lines
2170-2215 (inside the outer `++secp` door) implements RFC 6979
directly. Historical precedent: these two implementations have
drifted before. Key points of divergence candidates:

1. **HMAC-DRBG initialization**. RFC 6979 requires `V = 0x01*32` and
   `K = 0x00*32` as starting material, then bitstream-integer of
   (priv || hash || T) run through HMAC-SHA256. If the Hoon splits
   priv or hash as different bit-sizes from libsecp256k1, the HMAC
   inputs diverge.

2. **Hash truncation / bit-to-field conversion**. RFC 6979 §2.3.2
   specifies `int2octets` and `bits2octets` — converting the hash
   from bitstring to `[0, n-1]` integer. Edge cases when the hash
   has high bits set.

3. **Input size enforcement**. zuse.hoon's `make-k` probably does
   `?> (gte 32 (met 3 hash))` and similar; libsecp256k1 expects
   exactly 32 bytes. If padding/truncation differs, the inputs to
   RFC 6979 differ.

4. **Re-roll loop bias**. If the initial nonce falls outside `[1, n-1]`
   or would produce a degenerate r, RFC 6979 re-runs HMAC-DRBG. If
   one implementation advances the counter and the other doesn't,
   divergence.

## Reproducing

```bash
./fuzz/build.sh fuzz_jet_l13_asym
./fuzz/out/fuzz_jet_l13_asym.afl < fuzz/findings/023-ecdsa_make_k_mismatch/repro.bin
```

Expected: `test: make $: mismatch: good 4ebdf2dc, bad 324f2046` +
SIGABRT.

## Next steps

1. Run libsecp256k1's RFC 6979 test vectors through both sides to see
   which (if either) matches the authoritative reference.
2. Compare against Bitcoin Core or ethers.js signatures derived from
   the same (hash, priv).
3. If the C jet matches reference and Hoon doesn't, fix Hoon (like
   #018). If Hoon matches reference and jet doesn't, the C jet's
   `urcrypt_secp_sign` — or the flag bits we pass to it — is wrong.
4. Cross-check the fix against whatever signing path Urbit actually
   uses on-wire (Azimuth ownership claims, Ames handshake if any).

## Severity note

Urbit's ship-key mechanism uses ECDSA over secp256k1 at the Azimuth
layer. Divergence between C and Hoon signing means that a pure-Hoon
client (e.g. a Hoon sandbox without jets, or the Hoon reference run
at snapshot-restore time) would produce signatures that the C-jetted
path (running in vere) would reject, and vice versa. Any system that
compares signatures across the jet/Hoon boundary breaks.

This finding is a precondition for #021's full resolution.
