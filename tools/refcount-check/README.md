# refcount-check

Rust port of `tools/refcount_check.py`: a static checker that verifies
functions over `pkg/noun` follow the u3 reference-counting conventions
(transfer/retain protocols, `@Refcount:` annotations) documented in
`doc/spec/u3.md` and in the Python script's docstring.

The port is behavior-identical to the Python tool: same findings, same
output format, same exit codes, same `--selftest` / `--explain` /
`--function` / `--only` / `--verbose` flags.

## Build & run

```sh
cd tools/refcount-check && cargo build --release
# from the repo root (include paths in compile_commands.json are relative):
./tools/refcount-check/target/release/refcount-check \
    --libclang /usr/lib/llvm-19/lib/libclang.so
./tools/refcount-check/target/release/refcount-check --selftest ...
./tools/refcount-check/target/release/refcount-check --explain hashtable.c:u3h_put ...
```

Requires libclang 17+ (loaded at runtime via `--libclang` or auto-found
under `/usr/lib/llvm-*`) and a fresh `compile_commands.json`
(`zig build -Dgenerate-commands` after removing `.zig-cache`).

## Architecture — the interpreter is swappable

```
src/config.rs   tables: noun typedefs, noreturn fns, u3a_is_* guards,
                destructurers, c3y/c3n/direct-atom constants
src/ast.rs      thin wrapper over libclang (clang-sys, runtime-loaded)
                + generic AST utilities (unwrap_expr, decl_ref_name,
                int_literal_value, unary_op, binop kinds, is_noun_type)
src/sem.rs      protocol vocabulary and annotation layer: Mode, Sem,
                Finding, AssertMode; the @Refcount: grammar parser;
                prefix/position defaults; comment harvesting;
                decl-vs-def sync check; FileComments / block_asserts
src/interp.rs   THE ABSTRACT INTERPRETER (self-contained)
src/main.rs     driver: CLI, compile_commands.json handling, TU
                iteration, --explain, --selftest, output
```

`interp.rs` talks to the rest of the tool through exactly one entry
point and one trait, both defined in `interp.rs` itself:

```rust
/// Services the interpreter needs from the enclosing tool.
pub trait Host {
    /// Resolved refcount protocol of a callee (annotation + defaults).
    fn callee_sem(&mut self, callee: &Cursor) -> Rc<Sem>;
    /// `{ // @Refcount: assert ... }` annotations on a compound statement.
    fn block_asserts(&self, compound: &Cursor) -> Vec<(AssertMode, Vec<String>)>;
}

/// Check one function definition; returns the findings.
pub fn check_function(host: &mut dyn Host, fun: &Cursor, sem: &Sem) -> Vec<Finding>;
```

Everything else the interpreter uses is read-only vocabulary:
`ast::Cursor` (and the `ast` helper functions) to walk the AST,
`sem::{Sem, Mode, AssertMode, Finding}` as plain data, and the tables in
`config`. It never parses annotations, reads files, or touches the
compile database. To rewrite the interpreter, replace `interp.rs` with
any implementation of `check_function` with the same signature — the
driver, annotation grammar, and clang plumbing need no changes.

Interpreter internals (all private to `interp.rs`): the abstract value
lattice `St` (uninit / owned / borrowed / consumed / direct / unknown /
conflict / poisoned) with `merge_val`, environments as insertion-ordered
maps from variable (or `struct.member` path) to `Val`, the
`Flow { falls, brks, conts }` statement result, per-arm switch forking,
forward-goto parking, an 8-iteration loop fixpoint over `env_key`, and
GNU statement-expression value propagation.

## Fidelity notes

- Findings were validated by diffing the full `pkg/noun` run against the
  Python tool (identical, including order) and by `--selftest`.
- Python's dict-insertion-order semantics are preserved by using
  `IndexMap` for environments.
- One deliberate micro-divergence: inside a GNU statement-expression
  whose tail statement never falls through (dead code after `u3m_bail`
  etc.), the Python tool leaves the partially-mutated environment in
  place; this port restores the pre-statement environment instead.
