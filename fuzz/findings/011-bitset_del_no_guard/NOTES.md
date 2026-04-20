# Finding 011 — `bitset_del`: no runtime bounds check (UNREACHABLE)

**Status:** patched in `pkg/vere/io/mesa/bitset.c` as defensive
hygiene. **Severity downgraded to "dead code"** after reachability
audit. This is NOT a remote DoS as originally claimed.

## Why the original claim was wrong

My H21 harness fuzzed `bitset_put` / `bitset_del` / `bitset_has`
directly by driving fuzz-controlled indices into the API. That
tells us the functions don't validate their own arguments. It does
NOT tell us whether any production code path passes untrusted
arguments to them.

A grep for `bitset_del` across the entire vere tree turns up **zero
callers outside the self-test block**:

```
pkg/vere/io/mesa/bitset.h:22   declaration
pkg/vere/io/mesa/bitset.c:69   definition
pkg/vere/io/mesa/bitset.c:108  inside #ifdef BITSET_TEST
```

The function is unused. It's an API leftover that would abort if
anyone ever called it — but nobody does. There is no path from a
UDP packet to `bitset_del`.

## The fix is still worth keeping

The patch adds a `mem_w >= bit_u->len_w` guard that matches the
shape of `bitset_put`'s guard (post-#012 fix). Reasons to keep it:

1. If a future change starts calling `bitset_del` with fragment
   indices, it inherits the defensive behaviour for free.
2. It costs nothing: a single comparison.
3. It matches the intent of the assertion ("reject out-of-range
   indices") while not aborting the process.

But the patch should be framed as "tighten the API contract" in
any PR description, not "fix a remote DoS."

## Trigger (harness-level, not production)

The saved reproducer drives the H21 harness directly:

```
00000000: 13c0 13c0 c0c0 54c0 c1c0 7100 00
```

This works because H21's `main` loop reads opcodes and indices
from the fuzz input and feeds them straight into bitset functions.
It's not a UDP packet; it's a synthetic stream of "put/del/has"
operations.

## Lesson learned

Fuzzing an internal API in isolation is valid but produces
"argument validation" bugs, not "reachable DoS" bugs. I conflated
the two. Future harnesses that target internal helpers need to be
accompanied by a reachability check: "who calls this function with
untrusted arguments?" before claiming severity.

See also: #012, which has the same "fuzzing internals in isolation"
shape but a slightly more plausible path to reachability.

## Reproducing (harness only)

```bash
./fuzz/build.sh fuzz_mesa_bitset
./fuzz/out/fuzz_mesa_bitset.afl < fuzz/findings/011-bitset_del_no_guard/repro.bin
```

Pre-fix: `u3_assert(mem_w < bit_u->len_w)` abort.
Post-fix: harness completes cleanly.
