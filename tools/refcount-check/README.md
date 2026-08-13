# refcount-check

A static checker that verifies functions over `pkg/noun` and `pkg/vere`
follow the u3 reference-counting conventions (transfer/retain protocols,
`@Refcount:` annotations) documented in `doc/spec/u3.md`.

## Build & run

```sh
cd tools/refcount-check && cargo build --release
# from the repo root (include paths in compile_commands.json are relative):
# --libclang flag is optional
./tools/refcount-check/target/release/refcount-check \
    --libclang /usr/lib/llvm-19/lib/libclang.so
./tools/refcount-check/target/release/refcount-check --selftest ...
./tools/refcount-check/target/release/refcount-check --explain hashtable.c:u3h_put ...
```

Requires libclang 19 (loaded at runtime via `--libclang` or auto-found
under `/usr/lib/llvm-*`) and a fresh `compile_commands.json`
(`zig build -Dgenerate-commands`). Older libclangs load (17+), but 18
anchors macro-expanded operator cursors differently, defeating the
macro-origin exemption for `u3a_to_ptr`-style word punning and raising
false `[strange expression]` findings; CI pins 19 for this reason.

By default the shared headers are precompiled once per run and every
translation unit is parsed against its group's PCH (about 2x faster,
since header parsing dominates the runtime). Each compile-flags group
tries the candidate prefix headers richest-first: `pkg/vere` groups get
`vere.h` + `noun.h` + `jets/w.h`, while `pkg/noun` groups cannot resolve
`vere.h` and fall back to `noun.h` + `jets/w.h`. `--no-pch` disables all
of this and parses every TU from scratch.

## What it does

This tool walks the functions (in `pkg/noun` and `pkg/vere` by default;
`--filter` is repeatable and takes comma-separated substrings to narrow
or change the scope) and checks whether their definitions satisfy u3
reference-counting conventions plus the validity of additional
annotations. The check is done by running an abstract interpreter against the body of the function.
Test harnesses (`*_test.c`, `*_tests.c`, `benchmarks.c`) are excluded,
as are the generated data blobs `ivory.c` and `ca_bundle.c`.

`pkg/vere` notes:

- The compile db resolves the noun headers through `.zig-cache` snapshot
  dirs; the checker substitutes the live `pkg/noun` sources in the same
  include-search position, so `@Refcount:` edits in noun headers apply
  immediately (no cache rebuild needed).
- `-Werror` is dropped from the lint parse: the `U3_REFCOUNT_LINT` macro
  swaps can raise warnings the real build does not.
- A call through a function pointer (driver callbacks, vtables) is
  modeled as TRANSFER: noun arguments are consumed, a noun product is
  owned by the caller. Callback implementations must follow transfer
  protocol; deliberate exceptions (e.g. `_pier_on_lord_live`) are
  annotated `retains` at the definition and the convention mismatch is
  tracked at the call site.

I aimed at ~0% false negative rate, given that the code being checked at least compiles, so the interpreter is quite strict, and it will complain about noun pointers or complex struct initialization, which it does not model for now.

Currently it checks refcounting correctness and reference liveness correctness, including modelling unifying equality effects. It also complains if a u3_noun is used in integer arithmetic without checking if it is a direct atom.

### u3_weak / u3_none checking

Declared types are contracts: `u3_weak` means "valid noun OR u3_none",
every other noun typedef promises a valid noun. The checker tracks
possibly-none-ness per value ([u3_none] findings):

- sources: products of functions declared to return `u3_weak` (through
  function pointers too), `u3_weak` parameters, fills through `u3_weak*`
  out-params, the `u3_none` literal, and conditionals with a `u3_none`
  arm;
- sinks (strict, at every binding): a possibly-none value must not be
  bound to / assigned to / stored through / passed as / returned as a
  non-weak noun type. `u3k` of a possibly-none value is always an error
  (`u3a_gain` asserts on `u3_none`), as are `u3h`/`u3t`, the
  `u3a_is_*` guards (`u3a_is_dog(u3_none)` is yes), and destructurers;
- refinement: comparing `!= u3_none` proves the value valid on that
  branch (`== u3_none` proves it valid on the other); comparing equal
  to any valid literal or to a proven-valid value does too. A function
  whose signature returns a non-weak noun type blesses its passthrough
  product -- this is how `u3x_good` works, with no special-casing: its
  own body is checked against its `u3_noun` signature;
- exemption: `u3z`/`u3a_lose` of a possibly-none value is tolerated by
  default -- it is a de-facto safe no-op (`u3a_north/south_is_normal`
  return `c3n` for `u3_none`). The `--strict-weak` flag turns it into
  a finding too.

## U3 refcount protocol conventions, extended

To quote u3.md:

> The `u3` convention is that, unless otherwise specified, *all 
functions have transfer semantics* - with the exception of the
prefixes: `u3r`, `u3x`, `u3z`, `u3q` and `u3w`.  Also, within
jet directories `a` through `f` (but not `g`), internal functions
retain (for historical reasons).

This linter follows that rule EXCEPT "internal functions" (interpreted as `static` functions and functions that start with `_`) retain in ALL jet directories. It additionaly refines the transfer semantics for the products of the functions:

- `u3r_`/`u3x_`-functions: retain arguments AND the product. The caller gets a borrowed value and must not free it;
- `u3z_`-functions, `u3q/u3w` jet functions: retain arguments, transfer product: the caller needs to free/transfer the product of the function.

## Annotations

The refcount protocol can be refined further or asserted with `@Refcount: X` annotations.

Any clause may carry a prose remark: a parenthesized aside or a `-- ...`
trailer (`@Refcount: noreturn (bail_f never returns)`, `@Refcount: assert
transfer -- the kernel owns [roc]`). Remarks are stripped before parsing,
so they never affect the protocol or the decl/def sync rule.

- A refcount annotation of a function may appear in the header comment of a function definition, the header comment of a function declaration, or the same line as the function declaration if it spans for just one line:

```c
/*  @Refcount: retain
 *  (header comment)
*/
u3_noun
foo(u3_noun a)
{ ... }

u3_noun bar(u3_noun u3_noun); // @Refcount: transfer (same line for declarations is OK)
```

- A refcount annotation of a code block may appear on the same line as the opening brace:

```c
{  //  @Refcount: assert transfer (this assignement consumes)
  *ptr_u = u3k(u3h(list));
}
```

- A refcount annotation of a file may appear in the first 4KB of a file:

```c
//  @Refcount: all functions are custom unless asserted otherwise
```

- Unless there is ambiguity, @Refcount: X, Y, Z is same as:

```c
//  @Refcount: X
//  @Refcount: Y
//  @Refcount: Z
```

- The function refcount directives are applied in order top to bottom, with each directive changing the refcount protocol of the function. If there are conflicting directives then the last one wins, but a warning about conflicting directives is raised.

- A function must have the same directives across all its declarations and its definition.

- If a function has no directive, and the file is not custom, then its refcount protocol is governed by the rules layed out above, in [U3 refcount protocol conventions, extended](#u3-refcount-protocol-conventions-extended) section

- If a function has no directive, and the file is custom, the function follows "custom" transfer protocol. It can be only called by other custom functions and functions with asserted refcount protocol.

- List of refcount directives for a function (singular spellings `transfer`/`retain` are also accepted):

  - `assert`: the body of the function is not checked; the annotated (or default) protocol is trusted. May prefix another clause (`assert transfers product`) to declare and trust it in one go
  - `custom`: the function follows an unmodeled protocol; its body is not checked and it may be called only from other custom functions or functions whose protocol is asserted. No other directives may be present
  - `transfers product`: the product of the function is owned by the caller
  - `` transfers `x`, `y`, `z` ``: the function takes ownership of the listed arguments
  - `transfers arguments`: the function takes ownership of all arguments
  - `transfers`: the function takes ownership of all arguments and the product is owned by the caller
  - `retains ...`: as `transfers`, mutatis mutandis: the product is an uncounted reference, arguments are borrowed
  - `` conslike `x`, `y` ``/`conslike arguments`: like transfer but the consumed value becomes borrowed, not poisoned
  - `` passthrough `x` ``: identity: the product IS argument x, with unchanged ownership, counts are untouched
  - `` direct `x`, `y`, `z` ``: if the function returned, the listed arguments were direct atoms
  - `direct product`: the product is a direct atom
  - `direct arguments`/`direct`: if the function returned, all arguments were direct atoms
  - `` reads `x`, `y` ``: the listed pointer-to-noun parameters are read through without consuming the pointee
  - `` consumes `x`, `y` ``: one counted reference of the noun behind each listed pointer parameter is given away inside the call (a transferring read, or a u3z of the old value before a refill -- indistinguishable from the caller's side)
  - `` fills transferred `x`, `y` ``: the listed pointer parameters hold a fresh owned noun on return; the caller must consume it. Overwriting an unconsumed owned pointee without a `consumes` clause is reported as a leak
  - `` fills retained `x`, `y` ``: as above, but the new pointee is an uncounted view (tied to the call's noun arguments)
  - `` fills transferred|retained `x` on `c3y` ``: the fill happens exactly when the function returns the given loobean. The body is checked per exit path (which must return literal `c3y`/`c3n`). At the call site a transferred fill is deferred: the call's product must be compared against `c3y`/`c3n` directly (`if ( c3n == f(a, &out) )`), and the owned fill lands only on the matching branch. A retained fill is applied optimistically, u3r_cell-style: a claiming `c3y`/`c3n` comparison restores the variable's previous value on the branch where the fill never happened, and without one the optimistic fill simply stays. `u3r_p` &co are annotated this way
  - `noreturn`: calling this function ends execution (it exits or aborts); no argument accounting applies at its call sites. Inside its body leaks are tolerated (the process dies anyway) and owned parameters are modeled as borrowed views; a reachable return or fall-through is reported as an annotation error
  - `` doomed on `c3n` ``: an exit returning this loobean obliges the CALLER to die (boot failure); leaks and pointee contracts on such paths are not checked

- Function-pointer declarators (struct callback fields, pointer variables and parameters) may carry their own `@Refcount:` annotation, e.g. a trailing `//  @Refcount: retains` on a callback field in a struct typedef; calls through the pointer follow it. Without one, a call through a function pointer TRANSFERS: noun arguments are consumed and a noun product is owned by the caller. Argument modes on declarators are limited to transfer/retain/direct (named per the declarator's parameter names, or bare).

- Repeated `u3h`/`u3t` of the same noun value resolve to the SAME value (nouns are immutable), so `if ( !_(u3a_is_cat(u3h(oct))) ) u3m_bail(...); ... u3h(oct)` proves the later read direct.

  The pointee clauses compose: an in-place accumulator update is `` consumes `out`, fills transferred `out` ``.

- Slot pointers. The interpreter tracks `u3_noun*` locals and pointee-annotated parameters as *slot pointers*: `&var` of a tracked noun, plain pointer assignment (aliasing), reads (`*p`) and stores (`*p = x`) through them all resolve to the pointed-at slot. A call site may hand a pointee-annotated parameter either `&var` or a tracked slot pointer (so an accumulator out-param can be passed along recursively). A slot pointer that escapes anywhere else -- an unannotated parameter, a store to memory, a return value, a struct initializer -- is reported.

- `u3i_defcons` is modeled natively: the product is a fresh owned cell carrying two unfilled *holes*, and the `&ptr` arguments rebind those pointer variables to them.

- List of refcount directives for a code block:

  - `assert transfer`: every store in the block consumes the stored value. Useful for assignments to persistent data structures (`u3A->roc = u3k(...)`) or in defcons patterns
  - `assert transfer x y z`: on falling out of the block, one counted reference of each named variable (bare names, space-separated) has been consumed, on top of the block's visible effects. Useful when the transfer happens at a store site the interpreter cannot recognize as a transfer.
  - `assert direct x y z`: trusted claim, applied at block entry, that the named variables hold direct atoms -- for facts the checker cannot derive and no runtime check exists (e.g. an else-branch where a product is known to be `u3_nul`).

- Loobean destructurers (`u3r_cell` &co) fill their out-params only when they return `c3y`: when the call's product is compared against `c3y`/`c3n`, the failing branch keeps the variables' previous values (so a `u3_nul` initializer survives an unmatched guard). The `u3x_*` variants bail instead of returning, so their fills are unconditional.

- Join (control-flow merge) errors name the disagreeing paths: parked environments (break/continue/goto) carry the parking site, and if/else branches their source extents.

- List of refcount directives for a file:

  - `all functions are custom unless asserted otherwise`: every function in the file defaults to the custom protocol and no bodies are checked (the whole phrase must be on one line with the `@Refcount:` tag)