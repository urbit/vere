# Finding 022 — `_cj_kick_z` mug-on-none SEGV (oracle infra bug, not jet bug)

**Status:** fuzzer-harness infra bug. `fuzz_jet_l15_misc` crashed SIGSEGV
at `pkg/noun/retrieve.c:1765` when `u3r_mug` was called on `u3_none`
(the sentinel 0xFFFFFFFF atom value).

**Not a real jet finding** — the jet/Hoon may have agreed fine; the
crash happened in the oracle's mismatch-reporting code when it tried
to mug a result that had somehow become `u3_none`.

## Stack

```
#0 _cr_mug_next at pkg/noun/retrieve.c:1765 (veb=0xFFFFFFFF)
#1 u3r_mug at pkg/noun/retrieve.c:1824
#2 _cj_kick_z at pkg/noun/jets.c:978   ← u3r_mug(ame)
#3 u3j_kick
#4 u3n_slam_on (gat=…, sam=0xFF)
```

Repro: 2 bytes `10 ff` → mode=16%17=16 (last arm, which is `de:json:html`
in l15_misc's table). Sample is `0xff` (1 byte = @t "cord").

## Root cause

In `pkg/noun/jets.c:974-984` (my hex-dump additions):

```c
if ( c3n == u3r_sing(ame, pro) ) {
  u3l_log("... good %x, bad %x",
         u3r_mug(ame),   // line 978 — crashes if ame == u3_none
         u3r_mug(pro));
  ...
}
```

If `_cj_soft` returned `u3_none` (meaning the Hoon formula itself
produced no value — punt from within), `u3r_sing(u3_none, pro)` is
already undefined behavior, and the subsequent `u3r_mug(u3_none)`
dereferences an invalid pointer.

## Fix

Guard the mug call:

```c
if ( u3_none == ame || u3_none == pro ) {
  u3l_log("test: %s %s: mismatch: ame=%s pro=%s",
         cop_u->cos_c, fcs,
         (u3_none == ame) ? "<none>" : "<atom>",
         (u3_none == pro) ? "<none>" : "<atom>");
} else {
  u3l_log("... good %x, bad %x", u3r_mug(ame), u3r_mug(pro));
}
```

Low priority — doesn't invalidate any finding, just protects the
oracle from its own diagnostic code. Fix opportunistically.

## Reproducing

```bash
./fuzz/build.sh fuzz_jet_l15_misc
./fuzz/out/fuzz_jet_l15_misc.afl < fuzz/findings/022-oracle_mug_segv_on_none/repro.bin
```

Expected: SIGSEGV in `_cr_mug_next`.
