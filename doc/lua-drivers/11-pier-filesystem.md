# Step 11 — Pier filesystem access

**Status:** ✅ done & verified (unit + live)
**Goal:** Let a driver read, write, and inspect files under its own pier — for
config, persistent state, scratch space, or manipulating pier contents.

## The `ctx` filesystem API

All paths are **relative to the pier root**; absolute paths and any `.`/`..`
component are rejected, so a driver can't escape the pier.

| Method | Returns | Meaning |
|--------|---------|---------|
| `ctx:pier_path()` | string | absolute path of the pier directory |
| `ctx:read(path)` | string\|nil | file contents (nil if unreadable) |
| `ctx:write(path, data)` | — | write/overwrite a file |
| `ctx:list([path])` | array | entry names in a directory (default: pier root) |
| `ctx:stat(path)` | table\|nil | `{kind="file"\|"dir", size, mtime}` |
| `ctx:exists(path)` | bool | does the path exist |
| `ctx:mkdir(path)` | — | create a directory (no-op if present) |
| `ctx:remove(path)` | — | delete a file or empty directory |

Bad/unsafe paths raise a Lua error; missing files return `nil` (read/stat) so
drivers can probe without `pcall`.

## Scope & safety

A driver is **trusted code the user dropped in** (closer to a kernel module than
a sandboxed plugin), so it gets the **whole pier**, not a sub-jail. The guard is
only against *escaping* the pier (`..`, leading `/`). Writing under `.urb/` (the
event log, checkpoints) can corrupt the ship — the docstring and this note say so;
that's the user's call.

Two practical cautions:
- **Don't write into the watched `lua/` folder** from a driver — it triggers a
  hot reload (step 09). Use a sibling dir like `lua-data/` (the example does).
- File ops are **synchronous** on the king loop. Fine for config-sized files;
  for large/slow IO a future async (`uv_fs_*`) variant would avoid blocking.

## Path resolution

`_fs_path_safe` walks the path components and rejects `.`/`..`/leading-slash;
`_fs_resolve` joins a safe relative path onto `pier->pax_c` (with `""`/`"."`
meaning the pier root). Everything bottoms out in plain POSIX
(`fopen`/`opendir`/`stat`/`mkdir`/`unlink`/`rmdir`) — Windows portability is a
follow-on.

## Files

| File | Change |
|------|--------|
| `pkg/vere/lua.c` | fs section (`_fs_path_safe`/`_fs_resolve` + 8 `ctx` methods), registered in `_ctx_methods`; `#include <sys/stat.h>,<dirent.h>,<unistd.h>,<errno.h>` |
| `pkg/vere/lua_tests.c` | `_test_fs` (against a fake pier root) |
| `doc/lua-drivers/examples/` | `60-fs.lua` |

## Verification

**Unit** (`zig build lua-test`): with a fake pier root, a driver writes/reads a
file, makes a nested dir + file, lists, stats, has `../escape` and `/etc/passwd`
**rejected**, removes, and reads `pier_path()` — all pass.

**Live** (fake `~nut` ship):
```
lua: fs: pier root is .../boot2/pier
lua: fs: read back: written by a lua driver     # write -> read round-trip
lua: fs: pier root has 6 entries                # list
lua: fs: .urb is a dir                          # stat
```
…and `pier/lua-data/state.txt` is really on disk afterward.

## Recommended next IO (not yet built)

Ranked, from the discussion that motivated this step:

1. **Scry** — read Arvo's namespace (`.^`) from a driver; the read-side
   complement to `ctx:plan`. Highest value; vere already has the peek/`u3_pico`
   machinery.
2. **DNS + hostname TCP** — `uv_getaddrinfo` so `tcp_connect` takes hostnames.
3. **Child process / `exec`** — `uv_spawn` with piped stdio.
4. **Unix-domain sockets / named pipes** — `uv_pipe_t`, local IPC (à la `lick`).
5. **HTTP client** — high value, heavier (TLS); wrap the already-linked curl.
6. **Async fs + fs-watch** — `uv_fs_*` and a `ctx:watch(path, fn)`.
