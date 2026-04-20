# Finding 007 — `_ce_patch_read_control`: heap OOB read on undersized control.bin

**Status:** discovered by H12 on its very first seed input, before
any fuzzing started. Real heap-OOB bug.

**Severity:** out-of-bounds read on attacker-controlled data. Triggered
by any `control.bin` smaller than `sizeof(u3e_control)` (20 bytes on
LP64). On-disk threat model: a corrupted or crafted snapshot
checkpoint dir crashes the ship at startup.

## Trigger

Any `.urb/chk/control.bin` whose size is in the range
`(0, sizeof(u3e_control))`. Smallest distinct trigger is a 1-byte
file; my saved repro is 17 bytes (the harness writes the first 8
bytes of input to control.bin via its split logic — ASan reports
the read 4 bytes past an 8-byte region).

```
00000000: 8002 0000 0000 0000 0000 0000 0000 0000
00000010: 00
```

## Root cause

`pkg/noun/events.c:412-444`:

```c
static c3_o
_ce_patch_read_control(u3_ce_patch* pat_u)
{
  c3_w len_w;
  ...
  // fstat to get len_w
  if (0 == len_w) {
    return c3n;
  }

  pat_u->con_u = c3_malloc(len_w);
  if ( (len_w != read(pat_u->ctl_i, pat_u->con_u, len_w)) ||
        (len_w != sizeof(u3e_control) +
                  (pat_u->con_u->pgs_w * sizeof(u3e_line))) )      // <-- OOB
  {
    c3_free(pat_u->con_u);
    pat_u->con_u = 0;
    return c3n;
  }
  return c3y;
}
```

The check on line 437 (`pat_u->con_u->pgs_w`) reads field `pgs_w`
from the just-malloc'd buffer. `pgs_w` is at offset 16 in
`u3e_control`:

```c
typedef struct _u3e_control {
  u3e_version ver_w;   //   8 bytes (c3_d)
  c3_w        has_w;   //   4
  c3_w        tot_w;   //   4
  c3_w        pgs_w;   //   4   <-- offset 16
  u3e_line    mem_u[]; //
} u3e_control;
```

So `pgs_w` access requires the buffer to be at least 20 bytes long.
The function only validates `len_w != 0`, then mallocs `len_w` and
reads, then dereferences `pgs_w` without bounds-checking the field
position against `len_w`.

A `control.bin` of any size in `[1, 19]` bytes triggers a heap
buffer overflow read.

## ASan report

```
==NN==ERROR: AddressSanitizer: heap-buffer-overflow on address ...
READ of size 4 at ...
    #0 _ce_patch_read_control  pkg/noun/events.c:437
    #1 main                    fuzz/harnesses/fuzz_ce_patch_control.c:108

is located 4 bytes to the right of 8-byte region
allocated by:
    malloc
    _ce_patch_read_control     pkg/noun/events.c:434
```

## Production impact

`_ce_patch_read_control` is called during pier checkpoint replay
(`_ce_patch_open` → `_ce_patch_read_control`). Anyone who can write
to the pier's `.urb/chk/` directory (local filesystem access, an
SSH transfer, a corrupted backup, etc.) can trigger this on next
boot.

Severity is bounded by the threat model — if disk state is
considered trusted in your deployment, this is a "corrupted file
crashes startup" issue. Per FUZZING.md §1 we treat disk state as
untrusted, so this is a P0 finding.

## Suggested fix

Add a length sanity check before dereferencing `pgs_w`:

```c
pat_u->con_u = c3_malloc(len_w);
if ( (len_w != read(pat_u->ctl_i, pat_u->con_u, len_w)) ||
     (len_w < sizeof(u3e_control)) ||                       // <-- new
     (len_w != sizeof(u3e_control) +
               (pat_u->con_u->pgs_w * sizeof(u3e_line))) )
{
  c3_free(pat_u->con_u);
  pat_u->con_u = 0;
  return c3n;
}
```

The new clause `len_w < sizeof(u3e_control)` short-circuits the
`pgs_w` access. Order matters: C `||` evaluates left-to-right, so
the new check must come BEFORE the `pgs_w` arithmetic.

## Related observations

The function also has a stale `u3_assert(0)` at line 424 (the
fstat-failure path), which would abort production rather than
returning c3n cleanly. That's a separate cleanup but worth noting.

## Reproducing

```bash
./fuzz/build.sh fuzz_ce_patch_control
./fuzz/out/fuzz_ce_patch_control.afl < fuzz/findings/007-ce_patch_read_control_oob/repro.bin
```

Expect `AddressSanitizer: heap-buffer-overflow` in
`_ce_patch_read_control`.
