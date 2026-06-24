# Vere microbenchmark — `perf` profile baseline (2026-06-24)

The runtime-internals half of the baseline (complements the timing table in
`baseline-2026-06-24.md`). Captured with Linux `perf` after enabling
unprivileged sampling (`sudo sysctl kernel.perf_event_paranoid=1`). Same
provenance as the timing baseline: commit `02b4ed7db0`, ReleaseFast,
`native-musl`, `-fno-omit-frame-pointer -g3`, Ryzen 7800X3D. Non-Tracy build.

Raw artifacts: `raw/perf-stat.txt`, `raw/perf-report-self.txt`,
`raw/perf-report-children.txt`. Reproduce per `README.md` → Profiling → perf.

## Hardware counters (`perf stat -d -d -d`, whole suite)

| metric | value | reading |
|---|---|---|
| task-clock | 1571 ms | — |
| user / sys | **0.49 s / 1.08 s** | **most wall time is in the kernel** |
| page-faults | **875,408** (557 K/s) | demand-faulting fresh loom pages during heavy alloc |
| instructions | 13.34 B | — |
| IPC | 1.70 | mediocre |
| frontend cycles idle | **48.5%** | heavily **frontend-bound** (I-cache/decode/branch) |
| branch-misses | **5.09%** of branches (134 M) | interpreter dispatch mispredicts |
| L1-d miss | 2.45% | moderate |
| dTLB-load-miss | **18.3%** | high TLB pressure (large loom working set) |

Two structural costs dominate:

1. **Memory / kernel (sys 1.08 s ≫ user 0.49 s, 875 K page-faults).** The
   `cons` / `re-cons` phases build and free large nouns, so the loom grows and
   the kernel faults in pages (and `munmap`/`mmap` churns). This is the
   memory-pressure story — and it is *most* of the wall clock in this suite.
2. **Frontend-bound interpreter (48.5% frontend idle, 5.1% branch-miss).**
   Classic bytecode-dispatch signature (computed-goto + data-dependent
   branches).

## Where cycles burn — self time (`perf report --no-children`)

```
 6.02%  u3i_edit              opcode-10 edit loop (the only interpreter-heavy phase here)
 4.15%  calloc                allocation — much via ur_cue_init_with (cue dict/buffers)
 1.41%  memset                zero-fill (calloc + bitstream output buffer)
 1.16%  u3a_celloc            loom cell allocation
 1.08%  ur_dict64_put         cue backref hashtable insert (self; ~21% incl. children)
 0.86%  __libc_malloc_impl    off-loom malloc
 0.79%  ur_bsr_tag            bitstream tag read
 0.74%  MurmurHash3_x86_32    jam/cue dedup + page checksum hash
 ~1-2% each: [k] unresolved   kernel page-fault / mmap paths (sum is large)
```

(Kernel frames are unresolved — `kptr_restrict`. Their large aggregate self
time = the page-fault/mmap cost noted above.)

## Top stacks — children time (`perf report`)

`_cue_bench` 38% → `ur_cue` 29% / `ur_dict64_put` 22% / `ur_cue_init_with` 11%
(`calloc`), plus `__munmap` 24% and the kernel fault paths. I.e. cue is
allocation- and hashtable-bound; jam similar at smaller scale.

## Cross-reference to `PERFORMANCE_REPORT.md`

The profile independently corroborates several findings:

| observed | report finding |
|---|---|
| `ur_dict64_put` hot; `MurmurHash3` in the hot set | **S1** (modulo in dict probe), **S2** (cue hashes integer keys via Murmur) |
| `calloc` + `memset` prominent in cue | **F7** (output buffer zero-filled), **F9** (cue doesn't pre-size dict) |
| 875 K page-faults, sys-bound, `munmap` churn | **M2** (loom never returns pages / alloc churn), **§3** memory pressure |
| 48.5% frontend-bound, 5.1% branch-miss | **N3–N5** (dispatch specialization / superinstructions), **B1/B2** (LTO, cpu baseline) |
| `MurmurHash3_x86_32` on 64-bit | **P7** (swap Murmur for crc32c/xxHash3) |
| legacy `u3s_cue` ≈2.5× `u3s_cue_xeno`/call (Tracy) | **S7** (retire legacy cue) |

## Caveats

- This suite is **jam/cue/edit-heavy and allocation-dominated**; it is *not*
  representative of steady-state interpreter throughput. The Nock bytecode
  dispatch loop (`_n_burn`), jet dispatch, and snapshot/boot are not exercised
  here — expand the suite (see `baseline-2026-06-24.md` → coverage gaps) before
  drawing interpreter conclusions from the 48.5% frontend figure.
- Sampling at 999 Hz; single run. Re-profile ≥3× and compare self-time deltas
  larger than ~0.5% absolute.
- For per-function data inside the Tracy GUI instead of perf, the build now also
  supports sampled callstacks: `zig build benchmarks -Dtracy=true
  -Dtracy-callstack=true` (also needs `perf_event_paranoid ≤ 1`).
