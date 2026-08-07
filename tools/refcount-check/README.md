# refcount-check

A static checker that verifies functions over `pkg/noun` follow the u3
reference-counting conventions (transfer/retain protocols, `@Refcount:`
annotations) documented in `doc/spec/u3.md`.

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

Requires libclang 17+ (loaded at runtime via `--libclang` or auto-found
under `/usr/lib/llvm-*`) and a fresh `compile_commands.json`
(`zig build -Dgenerate-commands`).

## What it does

This tool walks the functions (only in `pkg/noun` for now) and checks whether their
definitions satisfy u3 reference-counting conventions plus the validity of additional
annotations. The check is done by running an abstract interpreter against the body of the function.

I aimed at ~0% false negative rate, given that the code being checked at least compiles, so the interpreter is quite strict, and it will complain about noun pointers or complex struct initialization, which it does not model for now.

Currently it checks refcounting correctness and reference liveness correctness, including modelling unifying equality effects. It also complains if a u3_noun is used in integer arithmetic without checking if it is a direct atom.

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
//  @Refcount: assert custom file
```

- Unless there is ambiguity, @Refcount: X, Y, Z is same as:

```c
//  @Refcount: X
//  @Refcount: Y
//  @Refcount: Z
```

- The function refcount directives are applied in order top to bottom, with each directive changing the refcount protocol of the function. If there are conflicting directives then the last one wins, but a warning about conflicting directives is raised.

- A function must have the same directives across all its declarations and its definition.

- If a function has no directive, and the file is not custom, then its refcount protocol is governed by the rules layed out above, in [U3 refcount protocol conventions, extended] section

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
  - `` destructures `x` ``: pointer-to-noun out-parameters are filled with borrowed views into the noun argument x, u3x_cell-style
  - `` read-only `x`, `y` ``: the listed pointer-to-noun parameters are only read.

- List of refcount directives for a code block:

  - `assert transfer`: every store in the block consumes the stored value. Useful for assignments to persistent data structures (`u3A->roc = u3k(...)`) or in defcons patterns
  - `assert transfer x y z`: on falling out of the block, one counted reference of each named variable (bare names, space-separated) has been consumed, on top of the block's visible effects. Useful when the transfer happens at a store site the interpreter cannot recognize as a transfer.

- List of refcount directives for a file:

  - `custom file`: every function in the file defaults to the custom protocol and no bodies are checked