# Fuzz Testing Plan for Vere (AFL++)

Status: proposal / draft. This document lays out a staged plan for bringing
[AFL++](https://aflplus.plus) to bear on the Vere runtime. The scope is broad
— parsing, I/O, the allocator, and Nock execution — but the work is broken
into harnesses that can be landed and run independently.

### Decisions locked in for this plan

| Area | Decision |
|---|---|
| Build integration | New `-Dfuzz` option in `build.zig` (swaps CC to `afl-clang-lto`, enables ASan+UBSan, sets `-DU3_FUZZ`) — single source of truth, no separate Makefile. |
| `SIGSEGV` handler | Compile-time gated: `#ifdef U3_FUZZ` around `sigsegv_install_handler` in `_cm_signals`. No runtime opt-out, no dead code path. |
| Disk threat model | On-disk bytes are **untrusted**. Event log (`u3_disk_sift`), patch files (`_ce_patch_*`), and snapshot migrations (`u3_v{1,2,3,4}_load`) are all **P0**. |
| Compute budget | Medium: 16–32 cores on a dedicated box. Overnight campaigns. PR smoke via GitHub Actions against regression corpus only. |
| Sanitizer default | ASan + UBSan combined, one variant per harness. No MSan / TSan / plain variants in phase 1. |
| Windows | **Out of scope.** AFL++ is Linux-first; `pkg/vere/platform/windows/*` does not get fuzz coverage. |
| Triage | Deferred — run ad-hoc until we see the rate, formalise rotation after first bugs land. |
| `u3_disk_sift` FIXME | File an issue, link from the harness README, do **not** block landing. The fuzzer will hit it on day one and that is fine. |

---

## 1. Why fuzz Vere, and why AFL++

Vere is a C runtime that ingests a lot of attacker-influenced bytes. Any bug
in a parser that takes network, disk, or IPC bytes is reachable by a malicious
peer, a corrupted event log, or a rogue mounted pier. The runtime also has
non-trivial custom infrastructure (the loom allocator, a guard-page /
libsigsegv integration, setjmp/longjmp-based error handling, a jet dispatch
table) which is exactly the kind of code where memory-safety bugs hide.

AFL++ is the right tool here:

- **Coverage-guided feedback** with LLVM edge instrumentation
  (`afl-clang-lto`) finds inputs that exercise new code paths without a
  hand-written grammar.
- **Persistent mode** (`__AFL_LOOP`) plus **deferred forkserver**
  (`__AFL_INIT`) let us pay the cost of `u3m_boot_lite` *once* per fuzzer
  process and then run thousands of iterations per second.
- **Sanitizer integration** (`AFL_USE_ASAN`, `AFL_USE_UBSAN`,
  `AFL_USE_MSAN`) turns latent UB into crashes the fuzzer can triage.
- **CMPLOG / redqueen** (`AFL_LLVM_CMPLOG=1`) defeats magic-value and
  checksum gates — relevant for mug checks on ames/mesa packets and the
  version word in newt headers.
- **Custom mutators** via the `AFL_CUSTOM_MUTATOR_LIBRARY` interface let us
  teach the fuzzer bit-stream boundaries (jam/cue) or valid mesa packet
  framing where pure bit-flipping is inefficient.
- **Parallel fuzzing** (`-M main -S s1 -S s2 ...`) and **corpus sharing**
  across instances scale to a whole box or cluster.
- **AFL++** also ships `afl-cmin` / `afl-tmin` for corpus & testcase
  minimisation, and has a mature crash-triage workflow we can pin to CI.

---

## 2. Attack-surface map

This is the catalogue of "untrusted bytes in" functions found by walking
`pkg/`. Each line is a candidate fuzz harness; priority is `P0`/`P1`/`P2`.

### 2.1 Serialization (jam / cue) — **P0**

| Function | File | Notes |
|---|---|---|
| `ur_cue` | `pkg/ur/serial.h` | Pure off-loom cue; no u3 state. Cleanest possible harness. |
| `ur_cue_test` | `pkg/ur/serial.h` | Same, parse-only (no allocation). Fast. |
| `ur_jam` / `ur_jam_unsafe` | `pkg/ur/serial.h` | Differential vs `ur_cue`. |
| `u3s_cue_bytes` | `pkg/noun/serial.c:817` | On-loom cue; requires `u3m_boot_lite`. |
| `u3s_cue_xeno`, `u3s_cue_xeno_with` | `pkg/noun/serial.c:702` | Off-loom dict; the same code path conn.c uses. |
| `u3s_jam_xeno`, `u3s_jam_fib` | `pkg/noun/serial.c` | For differential jam↔cue roundtrips. |
| `u3s_sift_ud_bytes` | `pkg/noun/serial.h:130` | Atom/`@ud` parser. |

The jam/cue layer is the highest-value P0 target: it is the single most
widely-used parser in the system, it runs on attacker-controlled bytes from
ames / newt / conn / disk / dawn, and it has the richest internal state
(back-references, hash consing, allocation).

### 2.2 Newt (king↔serf IPC framing) — **P0**

| Function | File | Notes |
|---|---|---|
| `u3_newt_decode` | `pkg/vere/newt.c:94` | 5-byte length-prefixed framing. Good target in isolation. |
| king→serf flow | `pkg/vere/lord.c`, `pkg/vere/mars.c` | Downstream of newt, cues the message body. Higher effort but more realistic. |

A malformed frame from a compromised serf must not corrupt the king, and
vice-versa. Existing unit tests in `pkg/vere/newt_tests.c` give us the
scaffolding.

### 2.3 Disk / event log — **P0**

| Function | File | Notes |
|---|---|---|
| `u3_disk_sift` | `pkg/vere/disk.c:307` | Parses a persisted event: mug word + body cue. The source even has a `// XX u3m_soft?` comment flagging that a bail inside cue is not caught here — a great starting point. |
| `_ce_patch_read_control` | `pkg/noun/events.c:415` | Parses `control.bin` from the loom checkpoint. |
| `_ce_patch_verify` | `pkg/noun/events.c:495` | Checks page count + mugs. |

### 2.4 Past / snapshot migrations — **P0**

| Function | File | Notes |
|---|---|---|
| `u3_v4_load` | `pkg/past/v4.c:811` | Interprets a mmap'd loom snapshot as `u3v_v4_home`. |
| `u3_v3_load`, `u3_v2_load`, `u3_v1_load` | `pkg/past/v{1,2,3}.c` | Older formats. Seldom run but still exist on disk. |
| `u3_migrate_v{2,3,4,5}` | `pkg/past/migrate_v*.c` | Format upgrade — reads bytes laid down by the previous version. |

These are harder to harness because the input is "a whole mmap'd snapshot"
rather than a byte buffer, but they read a *lot* of untrusted structure and
have historically been a source of bugs. Given the disk-untrusted threat
model, a crafted snapshot is in-scope: it can be dropped in place of a
legitimate pier, swapped over SSH, or produced by a buggy earlier version.
The harness maps the fuzz input as if it were the loom and calls the
loader directly.

### 2.5 Ames (classic UDP protocol) — **P0**

| Function | File | Notes |
|---|---|---|
| `_ames_hear` / `_ames_sift_head` | `pkg/vere/io/ames.c:1353, 2073` | Packet entry. `ames_tests.c` already `#include "./io/ames.c"` so the static helpers are directly callable from a harness using the same trick. |
| `_ames_sift_prel` | `pkg/vere/io/ames.c:412` | Prelude / ship identities. |
| `_fine_sift_wail` / `_fine_sift_purr` | `pkg/vere/io/ames.c:453` onward | Fine request/response bodies. |

### 2.6 Ames STUN — **P1**

| Function | File | Notes |
|---|---|---|
| `u3_stun_find_xor_mapped_address` | `pkg/vere/io/ames/stun.c` | STUN response parser; unit-tested in `ames_tests.c`. |
| `u3_stun_make_response` | same | Encoder side; good differential target. |

### 2.7 Mesa (new transport) — **P0**

| Function | File | Notes |
|---|---|---|
| `mesa_sift_pact_from_buf` | `pkg/vere/io/mesa/pact.c:937` | **Primary** mesa packet entry. Already has a standalone test binary (`pact_test.c`) that boots u3 with ivory and does roundtrip randomised testing. Wiring this to AFL++ is low-effort. |
| `mesa_sift_head` | `pkg/vere/io/mesa/pact.c:576` | |
| `_mesa_sift_name / _data / _page_pact / _peek_pact / _poke_pact` | `pkg/vere/io/mesa/pact.c:634–890` | Sub-parsers. |
| `lss_builder_ingest`, `lss_builder_transceive` | `pkg/vere/io/lss.c`, `lss.h` | Merkle / integrity layer used by mesa. |

Note: the top of `git log` is `mesa: fix out of bounds read in packet
parsing` — this is currently an active bug class, which is exactly why we
want coverage-guided fuzzing here.

### 2.8 Khan / conn — **P0**

| Function | File | Notes |
|---|---|---|
| `_conn_moor_poke` | `pkg/vere/io/conn.c:593` | Receives an IPC message, cues it via `u3s_cue_xeno`, expects `[rid [tag dat]]`, then dispatches by `tag` to fyrd / peek / peel / urth. The cue layer itself is covered by H4/H5; the dispatch + shape validation is unique surface. **Promoted to P0** after sweep — local-attacker reachable via the socket and dispatch is shallow. |
| `_conn_read_peel` | `pkg/vere/io/conn.c:493` | Sub-handler for the %peel response shape. Walks the request noun. |

### 2.9 HTTP / cttp — **P1**

| Function | File | Notes |
|---|---|---|
| h2o request glue | `pkg/vere/io/http.c` | Wraps inbound HTTP request bodies + headers into u3_nouns for eyre. Network-reachable. h2o itself is fuzzed upstream; the glue layer that builds nouns is unique. |
| `cttp.c` outbound glue | `pkg/vere/io/cttp.c` | Outbound HTTP responses come from URLs the ship requested — lower attacker control. Stays P2. |

### 2.10 Unix / clay sync — **P2**

`pkg/vere/io/unix.c` walks a mounted directory and mirrors it into clay. A
malicious mount (weird permissions, symlinks, huge paths, files whose
contents become nouns) could surface interesting bugs. Best handled by a
filesystem-oriented harness that generates directory trees from
fuzzer-controlled bytes.

### 2.11 Dawn / keyfile — **P0**

| Function | File | Notes |
|---|---|---|
| `u3_dawn_vent` | `pkg/vere/dawn.c:270` | Parses jam'd keyfile during boot to extract the ship's identity + sponsor chain. **Promoted to P0** because a corrupted keyfile blocks boot, the input is structured, and the parser walks pre-cued nouns (so the attack surface is _post-cue_ noun shape validation, not the underlying cue). |
| `_dawn_eth_rpc` | `pkg/vere/dawn.c:149` | JSON-RPC fetch from an Ethereum provider — depends on `json_de` (see §2.13) for response parsing. |

### 2.12 Lick / fore / hind / lord / mars — **P1**

| Function | File | Notes |
|---|---|---|
| `_fore_inject` / `_fore_import` | `pkg/vere/io/fore.c` | Reads a jam'd file from disk and injects it as an ovum. Untrusted-disk threat model. |
| Lick generic IPC | `pkg/vere/io/lick.c` | Newer named-port IPC mechanism (used by gall agents to talk to local processes). Same threat shape as conn. |
| Newt message handlers | `pkg/vere/lord.c`, `pkg/vere/mars.c` | Process noun contents AFTER newt framing (which H6 covers). King and serf trust each other partially. |
| Effect dispatch | `pkg/vere/auto.c` | Turns arvo effects into king-side actions. Internal trust boundary. |

### 2.13 Parser jets (`pkg/noun/jets/e/`) — **P0 batch**

These run as Nock jets but operate on atoms (byte arrays) the way C parsers
do, and Hoon code passes them adversarial bytes from the network all the
time (eyre, scry, gall agents). Each jet is a small, self-contained
target.

| Function | File | Why |
|---|---|---|
| `u3we_json_de` / `u3qe_json_de` | `pkg/noun/jets/e/json_de.c` | **JSON decoder.** Highest-yield single target in the codebase. Fed every JSON response from web requests, scry results, gall agent JSON. JSON parsers are notorious for OOB reads, integer overflows, recursion bombs. |
| `scot` / `slaw` / `scow` / `scr` | `pkg/noun/jets/e/{scot,slaw,scow,scr}.c` | Hoon's atom-to-text and text-to-atom converters: `@ud`, `@ux`, `@uv`, `@uw`, `@p` (ship names), `@da` (dates), `@dr` (durations), `@t` (cords). Multiple sub-parsers per file. Called whenever a printf renders or a ship name is parsed from text. |
| `urwasm` | `pkg/noun/jets/e/urwasm.c` | wasm3 wrapper. Loads & runs WebAssembly modules from bytes. Format is rich and the loader has lots of validation. |
| `zlib_de` | `pkg/noun/jets/e/zlib.c` | zlib decompression. Decompression bombs, malformed streams, history-window overflows. |
| `base32` / `base58` / `base64` decoders | `pkg/noun/jets/e/base.c` | Tight bit math, easy to off-by-one. |
| `parse` / `leer` / `lune` | `pkg/noun/jets/e/{parse,leer,lune}.c` | Generic text parsers. |
| `secp` / `ed_*` signature parsers | `pkg/noun/jets/e/secp.c`, `ed_*.c` | Crypto signature *parsing* surface (the verify primitives themselves are upstream-fuzzed). |
| Hash wrappers | `pkg/noun/jets/e/{shax,sha1,blake,keccak,ripe,argon2}.c` | Lower yield (thin wrappers around well-fuzzed libs). |

### 2.14 Non-goals

- **Nock evaluation** (`pkg/noun/nock.c`) is **derivatively** fuzzed via
  the cue harnesses: any input that `u3s_cue_bytes` accepts is then a
  legal pre-validated noun that nock can run. Direct Nock fuzzing is
  lower yield because the input space is enormous and the bug surface
  is well-trodden.
- **Terminal belt parsing** (`term.c`) and **behn** are too thin to
  prioritise.

---

## 3. Infrastructure hurdles (and how to defuse them)

These are the Vere-specific gotchas that will make a naive AFL++ build
misbehave. Each must be handled before harnesses are useful.

### 3.1 The loom, guard pages, and `SIGSEGV`

`u3m_boot_lite` → `u3m_init` → `_cm_signals` → `sigsegv_install_handler` in
`pkg/noun/manage.c:2415` installs a libsigsegv handler. Guard-page touches
during normal operation intentionally generate `SIGSEGV`, which the handler
consumes to extend/protect loom pages (`u3e_fault` in
`pkg/noun/events.c:231`).

This fights AFL++ and ASan, which want to see crashes as crashes. The
decision is **compile-time gating**: wrap the `sigsegv_install_handler`
call (and the Windows `AddVectoredExceptionHandler` call in the same
function) in `#ifndef U3_FUZZ`. Fuzz builds skip handler installation
entirely; `SIGSEGV` goes straight to ASan / the kernel default.

This is safer than a runtime opt-out because:

- there is no way a fuzz-config binary can be accidentally shipped with
  working signal handling,
- no dead code paths survive in the normal build, and
- harnesses cannot forget to call a disable function.

Fuzz harnesses use a fixed loom size and must not touch memory outside
it; loom-growth via guard-page trap is not a feature fuzz builds need.
Any access outside the mapped loom **should** crash loudly — that is
exactly what we want to find.

### 3.2 `u3m_bail` longjmps and persistent mode

On malformed input, `u3s_cue_*` calls `u3m_bail` which does a
`_longjmp(u3R->esc.buf, ...)` (`pkg/noun/manage.c:1004`). If the harness body
is inside `__AFL_LOOP`, an unhandled longjmp will skip the loop tail and
possibly break the forkserver protocol.

The fix is simple: wrap the per-iteration body in our own `setjmp`, or use
`u3m_soft` / `u3m_soft_run`. For pure parsers (ur cue / test) where there is
no u3 road to leap on, a local `setjmp` that sets `u3R->esc.buf` once is
enough. For mesa / conn-style harnesses that actually build nouns we should
use `u3m_soft` so the runtime's trap machinery behaves correctly.

Cues that longjmp out are **not crashes** — they are rejected inputs and
should not be reported as bugs. We must treat them as clean negative
returns.

### 3.3 Loom state accumulation between iterations

Persistent mode runs many inputs in one process. For pure `ur_cue` this is
fine (off-loom). For on-loom harnesses we must prevent state bleed:

- `u3m_reclaim()` between iterations, or
- snapshot `u3R` at `__AFL_INIT` and restore it per iteration, or
- use **deferred forkserver** (`__AFL_INIT` after boot) **without**
  persistent loop (`N=1`). Each child forks from a clean post-boot state.
  Slower (~forkserver throughput, still very fast) but eliminates state
  bleed entirely.

Recommendation: start with deferred-only (no persistent loop) for all
u3-using harnesses; switch individual harnesses to persistent mode after
proving stability. Off-loom harnesses (ur_cue, stun) use persistent mode
from day one.

### 3.4 Loom sizing

- `pkg/ur` needs zero loom.
- `pkg/noun` serial tests use `u3m_boot_lite(1 << 24)` = 16 MiB.
- `pkg/vere/io/mesa/pact_test.c` uses `u3m_boot_lite(1 << 26)` = 64 MiB
  because it also loads the ivory pill.

At 64 MiB × 64 parallel fuzzers = 4 GiB RSS, which is tolerable on a
reasonable box. We should still avoid going higher than we need.

### 3.5 Ivory pill

Mesa/conn harnesses need the ivory pill (`pkg/vere/ivory/ivory.{h,c}`) to
call `u3v_boot_lite(pil)` because packet parsing uses jets that need arvo
state. The pill is a build artefact, already linked into `libvere`, so
harnesses that link `libvere` get it for free.

### 3.6 Unbounded allocations from a crafted cue

`ur_cue` backref tables and loom allocations are bounded by input size but
can still be the fuzzer's best "DoS via 1 KB input". AFL++'s default memory
limit (`-m`) will catch this; we should set `-m 512` (MiB) for off-loom
harnesses and `-m none` plus a rlimit wrapper for on-loom harnesses.

### 3.7 `stack_size = 0` in the build

`build.zig` sets `urbit.stack_size = 0`. Fuzz harnesses should pin a large
stack (`ulimit -s unlimited` or a wrapper), because `u3n_nock_on` recursion
is deep.

---

## 4. Build integration

All fuzz work lives behind a new **`-Dfuzz`** option in `build.zig`. When
set, the build:

1. Swaps the C compiler to `afl-clang-lto` (or `afl-clang-fast` as
   fallback).
2. Defines `-DU3_FUZZ`, which gates out the libsigsegv handler install
   (§3.1) and the Windows vectored exception handler.
3. Adds `-fsanitize=address,undefined -fno-sanitize-recover=all
   -fno-omit-frame-pointer -O1 -g` to all urbit-owned packages (`pkg/c3`,
   `pkg/ur`, `pkg/noun`, `pkg/past`, `pkg/vere`).
4. Forces native target (fuzz builds don't cross-compile).
5. Skips third-party deps we don't care about (h2o, curl, libuv) where
   possible; parsers we target don't depend on them except where mesa /
   ames pulls them transitively.
6. Produces **two** binaries per harness: `harness.afl` (plain
   instrumented) and `harness.cmplog` (built with `AFL_LLVM_CMPLOG=1`)
   for `-c` / redqueen.

The zig build owns the whole fuzz pipeline — no separate Makefile, no
duplicated flag logic. New harnesses are added as entries in a
`fuzz_harnesses` table in `build.zig`, parallel to the existing `tests`
table.

Layout on disk:

```
build.zig                     # extended with fuzz_harnesses table
pkg/vere/fuzz/                # harness sources live alongside the lib they test
  fuzz_ur_cue.c
  fuzz_u3_cue_bytes.c
  fuzz_newt_decode.c
  fuzz_disk_sift.c
  fuzz_ce_patch.c
  fuzz_ames_packet.c
  fuzz_mesa_pact.c
  fuzz_stun.c
  fuzz_conn_poke.c
  fuzz_past_v4_load.c
  ...
fuzz/                         # out-of-tree assets (not built, not committed to pkg/)
  corpus/                     # seed inputs, grouped by harness
    ur_cue/
    mesa_pact/
    ...
  dicts/
    cue.dict
    mesa.dict
    ames.dict
    newt.dict
  scripts/
    run.sh                    # launches N parallel fuzzers with sensible knobs
    triage.sh                 # afl-tmin + dedupe + issue stub
    cmin.sh                   # afl-cmin on shared corpus
    replay.sh                 # regression-corpus replay for CI smoke
  findings/                   # crash drops, one subdir per bug
  regression/                 # minimised reproducers kept forever
  README.md
```

The corpus / scripts / findings directory lives at the repo root (not in
`pkg/`) because it's infrastructure, not library code, and because the
corpus is large and best handled by Git LFS or an S3 mirror rather than
tracked per-PR.

### 4.1 Essential compile flags (managed by zig, shown here for reference)

```
CC          := afl-clang-lto
CFLAGS      := -O1 -g -fno-omit-frame-pointer \
               -fsanitize=address,undefined \
               -fno-sanitize-recover=all \
               -DU3_FUZZ
AFL_LLVM_CMPLOG := 1   # second build variant for CMPLOG
```

At runtime: `AFL_USE_ASAN=1 AFL_USE_UBSAN=1 afl-fuzz -i fuzz/corpus/ur_cue
-o out/ur_cue -M main -c zig-out/fuzz/fuzz_ur_cue.cmplog --
zig-out/fuzz/fuzz_ur_cue.afl`.

---

## 5. Harness inventory (phase-ordered)

### Phase 1 — pure parsers (week 1, no u3 state)

Goal: land the infrastructure and prove the toolchain with the fastest,
simplest targets.

#### H1. `fuzz_ur_cue`

```c
#include "ur/ur.h"
__AFL_FUZZ_INIT();
int main(void) {
  __AFL_INIT();
  ur_root_t* r = ur_root_init();
  ur_cue_t*  c = ur_cue_init(r);
  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  while (__AFL_LOOP(10000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    ur_nref ref;
    (void)ur_cue_with(c, (uint64_t)len, buf, &ref);
  }
  return 0;
}
```

Seeds: jammed outputs of small nouns — `ur_jam` a set of known fixtures
(0, [0 0], atoms of various sizes, deep trees, back-referenced nouns).
Dictionary: the jam bit tags (`0`, `10`, `11`) at byte boundaries,
length prefixes of small numbers.

#### H2. `fuzz_ur_cue_test`

Same as H1 but using `ur_cue_test_with` — no allocation, ~5× faster, good
for raw coverage throughput.

#### H3. `fuzz_ur_jam_cue_diff`

Differential. Input bytes → cue → re-jam → assert round-trip equality.
Catches encoder/decoder drift. Requires an ok cue first, so gated by H1's
corpus.

### Phase 2 — u3 serial (week 2)

#### H4. `fuzz_u3_cue_bytes`

Boots the loom, wraps a `u3m_soft` around `u3s_cue_bytes`. Detects
differences between the ur layer and the u3 layer (the u3 layer adds hash
consing and trips through the cache, so it has its own bug surface).

#### H5. `fuzz_u3_cue_xeno`

Same input, but via `u3s_cue_xeno_with` — the path used by `conn.c`. We
expect identical results and use it as a differential oracle vs H4.

#### H6. `fuzz_newt_decode`

Targets `u3_newt_decode`. The harness maintains a live `u3_moat` across
iterations to also fuzz the streaming state machine (half-headers, body
continuations) — fuzzer input is a list of chunks whose boundaries are
chosen by a simple header byte.

### Phase 3 — packet parsers (weeks 3–4)

#### H7. `fuzz_mesa_sift_pact`

The high-value target given the active bug class. Models `pact_test.c`
`_setup()` exactly:

```c
/* Signal handler install is compile-gated by -DU3_FUZZ (§3.1); no
 * runtime disable needed. */
u3C.wag_w |= u3o_hashless;
u3m_boot_lite(1 << 26);
u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
u3_weak pil = u3s_cue_xeno_with(sil, u3_Ivory_pill_len, u3_Ivory_pill);
u3v_boot_lite(pil);
__AFL_INIT();
while (__AFL_LOOP(1000)) {
  int len = __AFL_FUZZ_TESTCASE_LEN;
  u3_mesa_pact pac = {0};
  (void)mesa_sift_pact_from_buf(&pac, __AFL_FUZZ_TESTCASE_BUF, len);
  mesa_free_pact(&pac);
  u3m_reclaim();
}
```

Seeds: `_test_rand_pact` output — generate 1k valid packets from the
existing test and save them. Dictionary: mesa header version bytes,
protocol/type enum values, path separators.

#### H8. `fuzz_ames_sift_packet`

The same trick `ames_tests.c` already uses (`#include "./io/ames.c"`) lets
us call `_ames_sift_head` / `_ames_sift_prel` / `_fine_sift_wail` on
attacker-controlled bytes. Seeds: capture a handful of real ames packets
between two fake ships; minimise with `afl-cmin`.

#### H9. `fuzz_stun_response`

Pure parser. Target: `u3_stun_find_xor_mapped_address`. Seeds: the existing
`_test_stun_addr_roundtrip` output.

#### H10. `fuzz_lss_ingest`

Target: `lss_builder_ingest` + `lss_builder_transceive`. Requires choosing
a `leaves` count from a header byte in the fuzz input.

### Phase 4 — event log & snapshot (P0, parallel with Phase 3)

Because the disk threat model is "untrusted", these harnesses run on the
same priority as mesa/ames and should land in the same sprint.

#### H11. `fuzz_disk_sift`

Wraps `u3_disk_sift` (`pkg/vere/disk.c:307`) in our own `setjmp` so the
`// XX u3m_soft?` longjmp (flagged as a known gap in the source) becomes
a counted "rejected input" rather than a runaway crash.

**Action:** file a GitHub issue against the FIXME, link it from the
harness README, and land the harness *without* fixing it. The fuzzer
will almost certainly find it on day one — that is the feature, not a
bug in our plan. Triage the finding via the normal path; the fix gets
its own PR.

#### H12. `fuzz_ce_patch_control`

Takes the fuzzer input, writes it as `control.bin` into a tmpfs dir,
creates a matching `memory.bin` from the remaining bytes, calls into the
`_ce_patch_read_control` + `_ce_patch_verify` chain (`pkg/noun/events.c:
415, 495`). Requires exposing those statics behind `#ifdef U3_FUZZ`.

#### H13. `fuzz_past_v4_load`  (P0, non-trivial)

Writes the fuzzer input as a mmap-sized region, points `u3_Loom_v4` at
it, calls `u3_v4_load` (`pkg/past/v4.c:811`). Non-trivial because the
loom is normally a process-wide mmap at a fixed address; the harness
builds an alternate loom region in user memory and swaps
`u3_Loom_v4` / `u3P.nor_u` for the duration of the call.

Also wrap: `u3_v3_load`, `u3_v2_load`, `u3_v1_load`, and each of
`u3_migrate_v{2,3,4,5}` — these share loader infrastructure so a single
harness with a 1-byte "pick version" tag can cover all of them.

### Phase 5 — phase-2 surfaces (post-survey)

After H1–H13 landed, a second sweep of pkg/vere and pkg/noun/jets/e
turned up additional high-value targets. These are the **"phase-2
batch"** — driven by the new entries in §2.8, §2.11, §2.13.

#### H14. `fuzz_json_de`

Targets `u3we_json_de` / `u3qe_json_de` (`pkg/noun/jets/e/json_de.c`)
— Hoon's JSON decoder. Highest-yield single target in the codebase
because it ingests bytes from the wide network via eyre, scry results,
and gall agent JSON.

The jet entry takes an atom (cord) of JSON bytes and returns either
the parsed structure or `~`. The harness wraps it in `u3m_soft` and
constructs the atom from the fuzz input.

#### H15. `fuzz_conn_poke`

Targets `_conn_moor_poke` (`pkg/vere/io/conn.c:593`) — the Khan IPC
dispatch after the `u3s_cue_xeno` step. The cue layer is already
covered by H4/H5; this harness exercises the post-cue noun-shape
validation and tag dispatch (`%fyrd`, `%peek`, `%peel`, `%urth`).

#### H16. `fuzz_scot_slaw`

Targets the Hoon literal parsers in `pkg/noun/jets/e/scot.c` and
`slaw.c`. These convert atoms to/from text representations (`@ud`,
`@ux`, `@uv`, `@uw`, `@p`, `@da`, `@dr`, `@t`). Multiple sub-parsers
in one file — the harness picks which to call based on a leading
input byte.

#### H17. `fuzz_dawn_vent`  (skipped — covered transitively)

`u3_dawn_vent` (`pkg/vere/dawn.c:270`) turned out not to be a
useful direct fuzz target. It's the boot-time orchestrator: it
calls `clan:title`, `point:give:dawn`, `veri:dawn`, etc. — all
Hoon kernel functions — and makes Ethereum JSON-RPC calls. There
is no isolated C parser to fuzz.

The actual parser surfaces dawn touches are already covered:
- jam'd keyfile bytes → cue (H4/H5)
- Ethereum JSON-RPC response → JSON parse (H14)
- Hoon-side keyfile validation → not C-fuzzable

Skipping H17. The slot is reserved in case dawn grows a new
C-side parser later.

#### H18. `fuzz_urwasm_load`  (skipped — too deep to isolate)

`u3we_lia_run_v1` (`pkg/noun/jets/e/urwasm.c:2223`) is the main
entry. It's a 700-line Hoon-driven dispatcher: extracts the wasm
binary from a Hoon-shaped sample, sets up wasm3 env+runtime,
parses+loads, runs functions, marshalls return values back to
nouns. The actual `m3_ParseModule` / `m3_LoadModule` calls are
buried 200+ lines into the dispatcher, behind sample-shape
validation that requires a real Hoon core.

Calling wasm3's parser directly would just fuzz wasm3 (which is
upstream-fuzzed), not the urbit glue.

Skipping H18 unless we can isolate the bytes-to-wasm3 boundary
into a wrapper. The slot is reserved.

#### H19. `fuzz_zlib_de`

Targets `u3we_zlib_de` / `u3qe_zlib_de` (`pkg/noun/jets/e/zlib.c`).
Decompression bombs, malformed streams, dictionary edge cases.

#### H20. `fuzz_base_decode`

Targets the base32/58/64 decoders in `pkg/noun/jets/e/base.c`.
Tight bit math, easy to off-by-one.

### Phase 6 — deferred / future

#### `fuzz_nock_from_cue`  (deferred)

Input → `u3s_cue_bytes` → `u3m_soft_run` of `u3n_nock_on`. Not high
priority because nock itself is the most-trodden code in the system
and bugs would more likely surface in cue (already fuzzed) or jets
(now fuzzed individually). Deferred until the parser harnesses
plateau.

#### Crypto jet harnesses  (P2)

For wrapper jets in `pkg/noun/jets/e/` around well-fuzzed crypto libs
(secp256k1, ed25519, blake3, sha2, keccak, aes, hmac, argon2). The
wrappers are thin and the bug surface is narrow. P2 — only land if
H14–H20 plateau and the box has spare cores.

---

## 6. Corpus and dictionaries

### 6.1 Seeding

For every harness, seed the corpus with:

1. **Fixtures from existing unit tests.** Each `*_tests.c` file already
   constructs canonical inputs (see `_test_jam_spec`, `_test_cue_spec`,
   `_test_rand_pact`, `_test_stun_addr_roundtrip`). Emit them to disk in a
   one-off helper.
2. **Captures from real runs.** For ames and mesa, run two fake ships
   locally and tcpdump the UDP traffic. Convert pcap to raw payloads.
3. **Generated edge cases.** Zero-length, one-byte, all-0x00, all-0xff,
   maximum-length near internal size caps.

Run `afl-cmin` on the combined corpus before launching fuzzers; it
typically cuts corpus size by 5–20×.

### 6.2 Dictionaries

- `cue.dict`: jam bit tags, common atom lengths, small back-reference
  offsets.
- `mesa.dict`: `MESA_`-prefixed constants from `pkg/vere/io/mesa/mesa.h`,
  packet type / protocol enum values, common path separators.
- `ames.dict`: version-byte + reserved-bit combinations, sac/rac rank
  bytes, known mug prefixes.
- `newt.dict`: the version byte `0x00`, length encodings of common message
  sizes (1, 64, 256, 4096).

Dictionaries compound with CMPLOG — don't skip either.

### 6.3 Corpus sharing

All fuzzers in a campaign share `out/` via AFL++ main/secondary
(`-M main -S s1 -S s2 ...`). One secondary per harness should run with
`AFL_DISABLE_TRIM=1` and `-p exploit` to keep raw throughput high, another
with CMPLOG, another with MOpt mutator (`-L 0`).

---

## 7. Sanitizers

**Phase 1 ships one variant per harness: ASan + UBSan combined.** Both
sanitizers live in the same binary; `-fno-sanitize-recover=all` means any
report is immediately fatal and therefore visible to the fuzzer as a
crash. This is the default for every harness listed in §5.

We explicitly do **not** ship MSan, TSan, or plain variants in phase 1:

- **MSan** catches uninitialised reads but is incompatible with ASan (the
  two cannot be linked together) and requires an MSan-clean libc — both
  of which are real work. Deferred; revisit once ASan+UBSan stops finding
  new bugs.
- **TSan** has no business on code paths that are all single-threaded —
  cue, disk_sift, mesa/ames parsers, etc. If we later write a threaded
  harness (unlikely), TSan can be added as a one-off.
- **Plain / no sanitizer** maximises exec/sec but misses the bugs we're
  looking for; the throughput win isn't worth the coverage loss.

The one exception: if the ASan+UBSan binary has stability issues with
persistent mode on a particular harness (stability < 95%), we may fall
back to plain for that harness while the stability problem is
investigated.

---

## 8. Running & triage

### 8.1 Local quickstart

```bash
# one-time
cd fuzz && make -j
mkdir -p out/cue_ur

# run (single-core)
AFL_USE_ASAN=1 \
AFL_AUTORESUME=1 \
afl-fuzz -i corpus/cue_ur -o out/cue_ur \
         -x dicts/cue.dict \
         -c build/asan/fuzz_ur_cue.cmplog \
         -m 512 -t 1000 \
         -- build/asan/fuzz_ur_cue
```

### 8.2 Parallel / box-wide

```bash
fuzz/scripts/run.sh cue_ur 16   # 1 main + 15 secondaries
```

The script pins each instance to a core, alternates persistent / CMPLOG /
MOpt secondaries, and rotates sanitizer variants every N hours via
`AFL_AUTORESUME=1`.

### 8.3 Triage

**Initial policy: ad-hoc.** Whoever runs the campaign owns the crashes
they find; no rotation, no SLA. The 16–32 core budget won't flood us
with new bugs once early wins are mined out. Revisit after the first
real crop of findings — if the rate justifies it, stand up a weekly
rotation among runtime maintainers.

Process per crash:

1. `afl-tmin` to minimise the reproducer.
2. `afl-showmap` to extract the coverage trace.
3. Rebuild the harness with symbolisation (ASan already on) and confirm
   the ASan report.
4. Bucket by stack-hash (ASan dedup tokens).
5. File a GitHub issue with the minimised testcase (base64 inline for
   small ones, attachment for larger), the harness name, the commit SHA,
   and the ASan report.
6. Drop the minimised reproducer into `fuzz/regression/<harness>/` and
   open the fix PR against that corpus so CI smoke proves the fix.

Keep `fuzz/findings/YYMMDD-harness-short/{input.bin, crash.txt, notes.md}`
as a per-bug scratch dir during triage; promote to `fuzz/regression/`
once minimised.

### 8.4 Regression corpus

Every bug, once fixed, contributes its minimised reproducer to a
permanent regression corpus (`fuzz/corpus/regression/<harness>/`). CI runs
`afl-showmap` against these on every PR to confirm they still pass — no
re-fuzzing needed, just deterministic replay.

---

## 9. CI integration

Two tiers, sized for the 16–32 core budget:

- **Per-PR smoke (GitHub Actions)**: replay the regression corpus
  (`fuzz/regression/`) through every harness. Target wall time: <60 s
  total. Blocks merge on any new crash or any regression failure. No
  mutation, no fuzzing — pure deterministic replay of known bad inputs.
- **Nightly (dedicated 16–32 core box)**: rsync the shared corpus from
  the prior run, launch one `-M main` + N-1 `-S` instances per P0
  harness (cycling harnesses through the cores so every harness gets
  air time over the week), run for ~8 h, then `afl-cmin` the corpus and
  push the minimised result + any new crashes back. Output: a Slack or
  GitHub-issue message listing new findings with ASan dedup hashes.

**Target host: this local box** — 16 cores (x86_64), 93 GiB RAM, 101 GiB
swap. This comfortably hosts 16 parallel on-loom harnesses plus ASan
overhead with RAM to spare; mesa harnesses at 64 MiB loom × 16 instances
= 1 GiB for looms, leaving ~90 GiB for ASan shadow memory, corpus
caching, etc. No cloud needed; nightly runs kick off via a local cron
or systemd timer. Campaigns can run around the clock without hitting
resource limits.

**Not in scope for phase 1:**

- ClusterFuzz Lite — attractive but adds a GitHub Actions dependency
  that's awkward for a 16–32 core nightly model. Revisit if we outgrow
  the single-box plan.
- OSS-Fuzz integration — requires upstream sign-off and ongoing
  maintenance; revisit after the in-house pipeline is stable.

---

## 10. Metrics and success criteria

Track, per harness:

- **Edges covered / total** (`afl-showmap -C`) — target ≥60% on the
  target file within the first week.
- **Stability** (`afl-fuzz` stability metric) — target ≥95%. If below,
  investigate state leak / TSAN-style nondeterminism.
- **Crashes found, deduplicated.**
- **Executions/sec**, per harness, persistent vs fork mode.

A campaign is "successful" when:

1. All P0 harnesses are instrumented, stable, and running in CI.
2. The regression corpus is >50 entries with zero replays failing.
3. Coverage on `pkg/ur/serial.c`, `pkg/noun/serial.c`,
   `pkg/vere/io/mesa/pact.c`, and `pkg/vere/newt.c` is above 70% edges.
4. Any new crashes go days between discoveries rather than minutes (the
   crash curve flattens).

---

## 11. Rough timeline

| Week | Deliverable |
|---|---|
| 1 | `-Dfuzz` in `build.zig`; `#ifdef U3_FUZZ` around `_cm_signals`; H1 `fuzz_ur_cue` running; `fuzz/` dir scaffolded |
| 2 | H2 `fuzz_ur_cue_test`, H3 `fuzz_ur_jam_cue_diff`, H4 `fuzz_u3_cue_bytes`, H5 `fuzz_u3_cue_xeno`, H6 `fuzz_newt_decode` |
| 3 | H7 `fuzz_mesa_sift_pact`; PR-smoke CI job (regression replay) wired up |
| 4 | H8 `fuzz_ames_sift_packet`, H9 `fuzz_stun_response`, H10 `fuzz_lss`, H11 `fuzz_disk_sift` (with linked issue for the `u3m_soft` FIXME) |
| 5 | H12 `fuzz_ce_patch_control`, H13 `fuzz_past_loads` (all versions behind a 1-byte selector) |
| 6 | H14 `fuzz_nock_from_cue`; nightly dedicated-box fuzzer up; first crop of triaged bugs |
| 7+ | Crypto-jet harnesses (P2); h2o / cttp glue (P2); re-evaluate triage rotation |

---

## 12. Decisions

Closed items from the earlier "open questions" section. Captured here
for future readers who want to know *why* the plan looks the way it
does.

| Question | Decision |
|---|---|
| Build integration location | **`-Dfuzz` in `build.zig`.** Single source of truth; new `fuzz_harnesses` table parallel to the `tests` table. No separate Makefile. |
| Signal handler disable mechanism | **Compile-time `#ifdef U3_FUZZ`** around `sigsegv_install_handler` (and the Windows `AddVectoredExceptionHandler`) in `_cm_signals`. No runtime opt-out. |
| On-disk threat model | **Untrusted.** Past / snapshot / event log are all P0. `fuzz_past_loads` promoted from P1 → P0. |
| Compute budget | **Medium: 16–32 cores on a dedicated box**, overnight campaigns. PR smoke is deterministic replay only, runs in GitHub Actions. |
| Sanitizers | **ASan + UBSan combined, one variant per harness.** No MSan / TSan / plain in phase 1. |
| Windows | **Out of scope.** `pkg/vere/platform/windows/*` does not get fuzz coverage. |
| Triage ownership | **Deferred / ad-hoc.** Whoever runs the campaign owns their crashes. Revisit rotation after first real crop of findings. |
| `u3_disk_sift` FIXME | **File an issue, link from harness README, do not block.** The harness lands with the FIXME still present; fuzzer finds it; fix is a separate PR. |

## 13. Remaining open questions

The following couldn't be resolved upfront and become decisions once we
start running:

1. **Ivory pill cycling.** Mesa harnesses load the committed ivory pill.
   When Arvo updates and the pill is regenerated, the mesa corpus may
   need re-validation (old fuzz inputs may parse differently against the
   new pill). We'll revisit the first time the pill changes — if it
   turns out to matter, we bake the pill's SHA into corpus filenames so
   stale inputs are auto-invalidated.
2. **Corpus storage.** The shared corpus will grow past what's
   reasonable to commit to git. Options: Git LFS, an S3 bucket rsynced
   nightly, or a separate `vere-fuzz-corpus` repo. Pick when corpus
   exceeds ~100 MB.
3. **CMPLOG cost.** Redqueen / CMPLOG adds ~2× CPU per instance. Worth
   it on parsers with magic values (mesa version bytes, newt header),
   probably not on pure bit-flipping targets (ur_cue). We'll tune
   per-harness after measuring.

---

## Appendix A. Minimal harness skeleton

```c
/* fuzz/targets/fuzz_u3_cue_bytes.c */
#include "noun.h"
#include <setjmp.h>

__AFL_FUZZ_INIT();

/* one-time, pre-forkserver */
static void
setup(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 24);
  /* Signal handler install is compile-gated by -DU3_FUZZ (§3.1). */
}

int
main(void)
{
  setup();
  __AFL_INIT();                 /* forkserver snapshot point */

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

  while (__AFL_LOOP(1000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    u3_noun why;
    if ( 0 == (why = (u3_noun)_setjmp(u3R->esc.buf)) ) {
      u3_noun out = u3s_cue_bytes((c3_d)len, buf);
      u3z(out);
    }
    /* else: cue bailed; this is a rejected input, not a crash. */
    u3m_reclaim();
  }
  return 0;
}
```
