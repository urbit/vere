# Optimization 01 — ThinLTO

Report finding **B1** (no LTO). Branch `mw/lto`. Determinism-safe: LTO is
inlining/codegen only — it does not change evaluation order or FP results; unit
tests pass unchanged. (The x86-64-v2 release-floor change, report B2, is tracked
separately on `mw/cpu-v2`.)

## What changed (`build.zig`)

**`-Dlto`** (new option; defaults on for `-Drelease`): sets `want_lto` on the
urbit-owned libraries (`c3`, `ur`, `noun`, `past`, `vere`) **and** the exe /
test exes, so ThinLTO inlines across the per-package `.a` boundaries. The hot
Nock loop can now inline the noun/alloc primitives (`u3a_*`, `u3i_*`, `u3h_*`)
that live in other translation units. Third-party libs (gmp, openssl, …) are
left alone.

## Result — benchmark suite (median of 10, ReleaseFast, Ryzen 7800X3D)

```
benchmark               noLTO    LTO   delta
--------------------------------------------
cue virtual og             50     36  -27.7%
cue og                     56     42  -25.9%
cue virtual atom           14     11  -21.4%
cue atom                   18     15  -16.7%
nock dec 200k loop         12     10  -16.7%   ← interpreter
ack(3,5)                  107     91  -15.0%   ← interpreter (deep recursion)
slam 500k                  54     46  -14.8%   ← nock-9 dispatch
jam xeno                    7      6  -14.3%
jam og                      8      7  -12.5%
cue xeno with              10      9  -10.0%
cons/free 100k x10         11     10   -9.1%   ← allocator
reap/free 100k x10         11     10   -9.1%
cue test                   16     15   -6.2%
cue xeno                   22     21   -4.5%
jam cons                  228    220   -3.3%
opcode 10 10k list         92     89   -3.3%
cue re-cons               598    593   -0.8%   ← page-fault/kernel bound; ~flat
cue cons                  416    414   -0.2%   ← page-fault/kernel bound; ~flat
```

The wins land exactly where B1 predicted: **compute-bound interpreter /
dispatch / small-op paths improve 9–28%** (the noun/alloc primitives now inline
into the hot loops). The two heavy allocation-bound phases (`cue cons` /
`re-cons`) stay flat — they are dominated by kernel page-fault/`mmap` time
(see `baseline-perf-2026-06-24.md`), which LTO cannot touch. Binary grows
~7% (13.2 → 14.2 MB).

## Result — warm jet dispatch (`urbit eval`, min of 5)

```
                  noLTO    LTO   Δcompute
eval startup        415    392
jetted dec 1M       558    521   143 → 129 ms  (-10%)
jetted ack(3,7)     549    508   134 → 116 ms  (-13%)
```

Consistent with the cold C `ack` (-15%): warm jetted recursion is ~13% faster.

## Correctness / determinism

- `nock-test`, `serial-test`, `jets-test`, `hashtable-test` all **pass** under
  `-Dlto=true`.
- The self-validating benchmarks (`dec(7)==6`, `ack(2,2)==7`, `slam(5)==6`)
  pass in the LTO build.
- The two-arch replay-mug determinism gate (`baseline-2026-06-24.md` §
  limitations / report §1.3) remains the belt-and-suspenders check before
  release.

## How to build

```sh
zig build -Doptimize=ReleaseFast -Dlto=true          # native, LTO
zig build -Drelease                                  # release: LTO on by default
zig build benchmarks -Doptimize=ReleaseFast -Dlto=true
```
