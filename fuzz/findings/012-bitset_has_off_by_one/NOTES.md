# Finding 012 — `bitset_has` / `bitset_put`: off-by-one guard (LIKELY UNREACHABLE)

**Status:** patched in `pkg/vere/io/mesa/bitset.c` as defensive
hygiene. **Severity downgraded to "theoretical"** after reachability
audit. The primary production call path is protected by an upstream
guard; one potential escape remains plausible but **not verified**.

## Why the original claim was wrong

My H21 harness fuzzed `bitset_has` / `bitset_put` directly. Those
ARE called from production code (`_mesa_req_pact_done` at
`pkg/vere/io/mesa.c:1141, 1159, 1181`) with wire-controlled
fragment indices. I assumed that was sufficient to call the
off-by-one a remote DoS. It's not, because there is an upstream
guard.

## The upstream guard

`pkg/vere/io/mesa.c:1132`:

```c
// received past the end of the message
if ( mesa_num_leaves(dat_u->tob_d) <= nam_u->fra_d ) {
  MESA_LOG(sam_u, STRANGE);
  return;
}

// received duplicate
if ( c3y == bitset_has(&req_u->was_u, nam_u->fra_d) ) { ... }
```

`req_u->was_u` is initialized at `mesa.c:2086`:

```c
bitset_init(&req_u->was_u, req_u->tof_d, &req_u->are_u);
```

with `req_u->tof_d = mesa_num_leaves(dat_u->tob_d)` from the first
packet of the request (`mesa.c:2080`).

If `req_u->tof_d == mesa_num_leaves(dat_u->tob_d)` every time
`bitset_has` is called, then the guard at 1132 ensures
`nam_u->fra_d < req_u->was_u.len_w` and the off-by-one cannot
fire via this path.

## The theoretical escape

The guard at 1132 uses the **current** packet's `dat_u->tob_d`.
The bitset was sized from the **first** packet's `dat_u->tob_d`
and stored in `req_u->tof_d`. If subsequent packets on the same
request can claim a different `tob_d`, the guard uses the new
value but the bitset has the old size.

Audit: grep for `tob_d != ` or `req_u->tob_d ==` in `mesa.c`
returns no matches. There is no code that rejects a subsequent
packet whose `tob_d` differs from `req_u->tob_d`.

Whether a subsequent packet with a different `tob_d` can actually
be routed to an existing `req_u` depends on how mesa matches
incoming packets to requests — something I did not fully trace.
If packets are matched by (sender, receiver, path, lanes) without
verifying `tob_d`, a crafted follow-up packet with a larger `tob_d`
and a fragment index in `[req_u->tof_d, mesa_num_leaves(new tob_d))`
passes the guard at 1132 and calls `bitset_has(was_u, fra_d)` with
`fra_d >= was_u.len_w`. Pre-fix this trips the `mem_w == len_w`
off-by-one.

**I did not verify this escape end-to-end.** H26
(`fuzz_mesa_page_flow`) is the attempt to do so.

## Related off-by-one in `bitset_put`

`bitset_put` had the same `>` vs `>=` off-by-one in its guard:

```c
if (( mem_w > bit_u->len_w )) {   // should have been >=
  u3l_log("overrun...");
  return;
}
```

With `bit_u->buf_y` having `(len_w >> 3) + 1` bytes allocated, a
`mem_w == len_w` write doesn't actually overrun the buffer — it
sets a bit at `byt_y[len_w >> 3] | (1 << (len_w & 7))`, which is
in-bounds thanks to the `+1` padding. Semantic bug, not
memory-safety. Fixed in the same patch for consistency.

## The fix is still worth keeping

Same reasoning as #011: cheap, defensive, aligns the API contract
with its invariants. Should NOT be framed as a DoS fix.

## Lesson learned

Fuzzing internal helpers found legitimate API hygiene issues, but
establishing reachability requires tracing the call graph from an
untrusted entry point to the helper. Harnesses that target
internal functions should be validated against "is there a path
from the wire / disk / IPC socket to this function with the
arguments I'm fuzzing?" before the findings are assigned severity.

## Reproducing (harness only)

```bash
./fuzz/build.sh fuzz_mesa_bitset
./fuzz/out/fuzz_mesa_bitset.afl < fuzz/findings/012-bitset_has_off_by_one/repro.bin
```

Pre-fix: `u3_assert(mem_w < bit_u->len_w)` abort.
Post-fix: harness logs overrun and continues.

## Reproducing the theoretical escape

Not demonstrated. See H26 `fuzz_mesa_page_flow` for the end-to-end
harness attempting to trigger the escape via two crafted mesa
page packets on the same request.
