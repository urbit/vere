# Finding 021 — `ecdsa-raw-sign:secp256k1:secp:crypto` oracle mismatch

**Status:** ORACLE MISMATCH detected by `fuzz_jet_l13_asym` in 41 min
of soak (2136 iterations). Jet and Hoon produce different signatures
on the same (hash, priv-key) input.

**Severity:** HIGH. ECDSA over secp256k1 is the signature algorithm
for Bitcoin, Ethereum (pre-EIP-4844), and every Urbit flow that
interacts with them. A C-vs-Hoon divergence means:
- Signatures produced by the jet don't match the Hoon reference.
- If either side is wrong, it could produce signatures that an
  external verifier (bitcoind, ethers, libsecp256k1) rejects — or
  that leak bits of the private key through deterministic-nonce
  divergence.

## Trigger

Repro (47 bytes):

```
fb 6e c0 21 6e b7 21 c0 02 03 04 6e c0 14 b7 6e
21 21 c0 14 c0 fb 6e 21 71 cc c0 c0 b3 09 50 01
01 c0 21 6e b7 02 03 04 af 09 c0 b3 09 50
```

- `buf[0] = 0xfb → mode = 251 % 5 = 1 → ecdsa-raw-sign`
- Remaining 46 bytes split: `hash = u3i_bytes(32, p)`,
  `priv = u3i_bytes(32, p+32)` where the zero-padded second atom uses
  only ~14 live bytes.

Oracle mugs: `good (hoon) 6c126f6d` vs `bad (jet) 5d68c7bf`.

I did not yet extract both sides' actual signature bytes — the
current `_cj_kick_z` hex-dump only fires for atoms, and sign returns
a cell `[v=@ r=@ s=@]`. Easy to enhance later.

## Hypothesis space

Potential root causes, any of which would need investigation:

1. **Nonce generation divergence**. `ecdsa-raw-sign` in `zuse.hoon:2248`
   uses `make-k` for deterministic nonce (RFC 6979 variant). If the
   C jet (`urcrypt_secp_sign` or similar) uses libsecp256k1's RFC
   6979, and the Hoon uses a custom scheme, they diverge.
2. **s-value canonicalization**. `zuse.hoon:2256-2264` applies
   "low-s" canonicalization (`s-high` check, reflect `s` and `v` if
   `2s > n`). If the C jet doesn't do this, or does it differently,
   signatures differ.
3. **v-byte computation**. Lines 2261-2263 derive `v` from `y mod 2`
   and whether `r >= n.domain`. Jet might use a different convention.
4. **Hash length handling**. `hash=@uvI` expects 32-byte atom; jet
   or Hoon might truncate/pad differently on short inputs.

## Reproducing

```bash
./fuzz/build.sh fuzz_jet_l13_asym
./fuzz/out/fuzz_jet_l13_asym.afl \
  < fuzz/findings/021-ecdsa_sign_mismatch/repro.bin
```

Expected: `test: sign $: mismatch: good 6c126f6d, bad 5d68c7bf` +
SIGABRT.

## Next steps

1. Extend `_cj_kick_z`'s hex-dump to handle cell results (dump each
   leg of `[v r s]` separately).
2. Parse both the jet and Hoon results, identify whether r, s, or v
   disagrees.
3. Compare the actual behavior against libsecp256k1's RFC 6979
   nonce — if the jet uses libsecp256k1 directly (urcrypt_secp_sign)
   and Hoon reimplements RFC 6979, reconcile.
4. Re-test with specific inputs (Bitcoin Core test vectors) to see
   which side matches a well-established reference.

## Why this matters

Ecdsa divergence in Urbit's stdlib has been a real historical issue.
The jet spec is strict: every input must produce the same result.
This is the kind of finding that motivated the ice-oracle campaign.

## Caveat

Given the size of potential attack surface (ECDSA signature
algorithm), this warrants careful triage before publishing. A false
positive here (e.g., the jet's output is the canonical one and
Hoon's is wrong) would be noise; a real divergence where the jet
produces an INVALID signature under an external verifier is a
shippable finding.
