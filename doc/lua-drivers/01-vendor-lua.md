# Step 01 — Vendor and link PUC-Lua 5.4

**Status:** ✅ done & verified
**Goal:** Make the PUC-Lua 5.4 C library available to (and linked into) the
vere/king binary, with zero references yet from vere's own code. This de-risks
the build/vendoring side before any bridge C is written.

## What vere's vendoring convention is

Third-party C libs live under `ext/<name>/` as a **mini Zig package** that does
*not* check in the upstream source. Instead:

- `ext/<name>/build.zig.zon` declares a `.dependencies.<name>` entry pointing at
  the upstream release tarball by `url` + content `hash`. Zig's package manager
  fetches and caches it.
- `ext/<name>/build.zig` compiles the fetched source into a static library
  artifact and installs its public headers under a namespaced subdir.

`ext/lmdb` is the closest template (single static lib, a couple of headers).

## Files added

```
ext/lua/build.zig.zon   # declares the lua-5.4.7 tarball dependency
ext/lua/build.zig       # compiles liblua.a, installs headers under lua/
```

### `ext/lua/build.zig.zon`
- Tarball: `https://www.lua.org/ftp/lua-5.4.7.tar.gz`
- Hash obtained with `zig fetch <url>` →
  `N-V-__8AAIMvFABt-Qcpk24RD10ldEN743D8Q2e19Er8x3dJ`
- `fingerprint` must be unique; Zig rejected a hand-picked value and printed the
  correct one to use (`0xd671372b08b211f6`).

### `ext/lua/build.zig`
- Mirrors `ext/lmdb/build.zig`: `addLibrary("lua")`, `linkLibC()`,
  `addIncludePath(src)`, `addCSourceFiles(...)`, `installHeader(...)`.
- Compiles the **core VM + standard library** `.c` files from `src/`, and
  **excludes the CLI mains** `lua.c` and `luac.c` (we embed the library only).
- Defines `-DLUA_USE_POSIX` on linux/macos. This turns on the POSIX feature set
  **without** `LUA_USE_DLOPEN`, so embedded Lua can't `require` native C modules
  — a small, deliberate sandboxing choice. (Which *Lua* std libs are exposed to
  a driver is a separate, runtime decision made later in the bridge.)
- Installs headers as `lua/lua.h`, `lua/lauxlib.h`, `lua/lualib.h`,
  `lua/luaconf.h`, `lua/lua.hpp`, so vere code will `#include <lua/lua.h>`.

## Files edited (wiring lua into the build graph)

Three build configs reference every dependency; lua was added to each next to
`lmdb` to keep the diff obvious:

| File | Edit |
|------|------|
| `build.zig.zon` (root) | add `.lua = .{ .path = "./ext/lua" }` |
| `build.zig` (root) | add `const lua = b.dependency("lua", …)`; `urbit.linkLibrary(lua.artifact("lua"))`; add `lua.artifact("lua")` to `vere_test_deps` |
| `pkg/vere/build.zig.zon` | add `.lua = .{ .path = "../../ext/lua" }` |
| `pkg/vere/build.zig` | add `const lua = b.dependency("lua", …)`; `pkg_vere.linkLibrary(lua.artifact("lua"))` |

The link is added at the `pkg_vere` layer (not just the final `urbit`) because
the Lua bridge code will live in `pkg/vere/` and the per-package test binaries
also link `pkg_vere`.

## Gotcha: Zig version

First build attempt used `/snap/bin/zig` = **0.16.0** and failed:
`no field or member function named 'linkLibC' in 'Build.Step.Compile'`. The
project pins **Zig 0.15.2** (`INSTALL.md`). The 0.16 `std.Build` API moved those
helpers off `Compile`, so *every* existing `ext/*/build.zig` would fail under it
too. Build with `/home/amadeo/zig-x86_64-linux-0.15.2/zig`.

## Verification

```sh
ZIG=/home/amadeo/zig-x86_64-linux-0.15.2/zig

# 1. library compiles standalone
cd ext/lua && $ZIG build
#   -> zig-out/lib/liblua.a + zig-out/include/lua/*.h

# 2. full runtime builds with lua in the link graph
cd ../.. && $ZIG build           # exit 0
#   -> zig-out/x86_64-linux-musl/urbit  (static musl, 41 MB)
#   -> liblua.a present in ./.zig-cache (pulled into the main graph)

# 3. binary runs
./zig-out/x86_64-linux-musl/urbit --version
#   urbit 4.6-257dbc2
#   gmp: 6.2.1
#   sigsegv: 2.14
```

Result: liblua builds, links cleanly into the runtime, and the binary still
runs. No vere code references Lua yet — that's step 02.

## Next

Step 02: the Lua↔noun bridge (`pkg/vere/lua.c` + `pkg/vere/lua.h`) — a noun
userdata type with `__gc`→`u3z`, and a `noun` module exposing build/inspect/
refcount primitives to scripts. This will be the first code to actually call
`luaL_newstate()` and prove the headers/symbols are reachable from vere.
