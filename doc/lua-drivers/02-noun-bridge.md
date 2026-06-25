# Step 02 — Lua ↔ noun bridge

**Status:** ✅ done & verified
**Goal:** Let Lua code build, inspect, and hold Urbit nouns safely, with Lua's
garbage collector and the noun reference-counter kept in sync. This is the first
code that actually calls into liblua, so it also proves the headers/symbols
wired up in step 01 are reachable from vere.

## Files added

```
pkg/vere/lua.h         # public bridge API
pkg/vere/lua.c         # noun userdata + `noun` module + sandboxed boot
pkg/vere/lua_tests.c   # unit tests (zig build lua-test)
```

## Files edited

| File | Edit |
|------|------|
| `pkg/vere/build.zig` | add `"lua.c"` to `c_source_files`; add `"lua.h"` to `install_headers` |
| `build.zig` (root) | register a `lua-test` entry in the `tests` array (uses `vere_test_deps`) |

`lua.h` is added to `install_headers` so that test/loader translation units can
`#include "lua.h"` — vere's test binaries resolve headers through the installed
header dirs of their dependency artifacts, not via a local include path.

## The model

### `u3.noun` userdata
A Lua full-userdata holds one `u3_noun` (which is just a `c3_w` loom offset).
Its metatable `"u3.noun"` defines:
- `__gc` → `u3z` the noun (released when Lua collects the handle),
- `__tostring` → `"<noun atom|cell mug=…>"`,
- `__eq` → `u3r_sing` (value equality).

### Reference-counting discipline (the sharp edge)
A noun userdata **owns exactly one** reference, released in `__gc`. Internally:
- `_push_owned(L, som)` — wrap a *freshly built* noun, taking its ref (no `u3k`).
- `u3_lua_push_noun(L, som)` — **RETAIN**: `u3k` then `_push_owned` the copy, so
  the caller keeps its own reference.
- `_check_noun(L, idx)` — return a **borrowed** ref (never `u3z` it).
- `_arg_noun(L, idx)` — return an **owned** (+1) ref, meant to be handed straight
  to a consuming constructor (`u3i_cell`). Coerces Lua int → atom, Lua string →
  cord, or `u3k`s a noun userdata.

This is what keeps the two memory managers from corrupting each other: every
noun that crosses into Lua is gained on the way in and lost at GC; every noun
pulled out for a constructor is balanced by that constructor consuming it.

## The `noun` module (Lua-visible API)

| Function | Meaning | Backed by |
|----------|---------|-----------|
| `noun.atom(int)` | non-negative atom | `u3i_chub` |
| `noun.cord(str)` | LSB-first byte atom (`@t`) | `u3i_bytes` |
| `noun.tape(str)` | tape (list of bytes) | `u3i_tape` |
| `noun.cell(a,b)` | the cell `[a b]` (args coerced) | `u3i_cell` |
| `noun.list(...)` | null-terminated list | `u3i_cell`×n |
| `noun.is_atom(n)` / `noun.is_cell(n)` | type test | `u3a_is_atom/cell` |
| `noun.head(n)` / `noun.tail(n)` | cell head/tail (errors on atom) | `u3h`/`u3t` |
| `noun.eq(a,b)` | value equality | `u3r_sing` |
| `noun.mug(n)` | 31-bit mug | `u3r_mug` |
| `noun.to_int(n)` | atom → Lua int, or nil if >64 bits | `u3r_chub`/`u3r_met` |
| `noun.to_string(n)` | cord atom → Lua string | `u3r_string` |

Args that take "a noun" accept either a `u3.noun` userdata or a plain Lua
int/string, so `noun.cell(2, "behn")` works.

## Sandboxing

`u3_lua_boot` opens only `base, table, string, math, coroutine, utf8` — **no**
`io`, `os`, or `package` — and nils out `dofile`/`loadfile`. Combined with the
`-DLUA_USE_POSIX` (no `dlopen`) build flag from step 01, an embedded driver
cannot touch the host filesystem or load native code. `load` is kept (the
loader in step 03 needs it; without dlopen it can't reach C).

## C API surface (lua.h)

```c
u3_lua* u3_lua_boot(struct _u3_pier* pir_u);   // state + noun module
void    u3_lua_halt(u3_lua* lua_u);            // lua_close + free
void    u3_lua_push_noun(lua_State* L, u3_noun som);  // RETAIN
u3_noun u3_lua_get_noun(lua_State* L, c3_i idx);      // borrowed
```

## Verification

```sh
ZIG=/home/amadeo/zig-x86_64-linux-0.15.2/zig
$ZIG build lua-test     # -> "test_lua: ok", exit 0
$ZIG build              # -> full runtime builds with lua.c, exit 0
./zig-out/x86_64-linux-musl/urbit --version   # still runs
```

`lua_tests.c` boots the loom (`u3m_init`/`u3m_pave`) and a bridge state, then:
- builds/inspects cells and atoms, checks `head`/`tail`/`to_int`;
- round-trips a cord through `to_string`;
- checks structural equality (both `noun.eq` and the `==`/`__eq` metamethod);
- checks list shape `[1 2 3 0]`;
- pushes a C-built noun into Lua and reads one back into C (`u3_lua_push_noun`
  / `u3_lua_get_noun`), confirming refcount balance;
- confirms misuse (`head` of an atom, negative atom) raises Lua errors instead
  of crashing;
- churns 10 000 cells through Lua and forces a full GC to exercise `__gc`.

## Next

Step 03 — the loader + meta-driver: `u3_lua_init(pir_u)` scans `$pier/lua/`,
`load`s each script (expecting a returned `{name, priority, talk, kick, exit}`
table), and wraps each in a `u3_auto` whose callbacks trampoline into Lua.
