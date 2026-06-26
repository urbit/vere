/// @file
///
/// Lua <-> noun bridge for vere.
///
/// Embeds a PUC-Lua 5.4 interpreter in the king process and exposes the noun
/// API to Lua scripts. This header is the public surface used by the loader
/// and the meta-driver (added in later steps).
///
/// NB: include this *after* "noun.h" / "vere.h" -- it relies on u3_noun, c3_i,
/// and the u3_pier forward declaration from those headers.

#ifndef U3_VERE_LUA_H
#define U3_VERE_LUA_H

#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>

  struct _u3_pier;
  struct _u3_auto;

  /* u3_lua: a Lua embedding state.
  **
  **   Each Lua driver owns its own isolated lua_State (booted by u3_lua_boot,
  **   closed by u3_lua_halt in the driver's exit trampoline), so one driver's
  **   globals or a crash cannot reach another's. Also used standalone in tests.
  */
    typedef struct _u3_lua {
      lua_State*       lua;                //  interpreter
      struct _u3_pier* pir_u;             //  pier backpointer (0 in unit tests)
      void*            emb_u;             //  luv embed harness (u3_lua_emb*), or 0
    } u3_lua;

  /* u3_lua_boot(): create a Lua state with the noun bridge registered.
  **
  **   Opens a sandboxed subset of the standard library (no io/os/package, no
  **   native module loading) and installs the global `noun` module plus the
  **   "u3.noun" userdata metatable. [pir_u] may be 0 (e.g. in tests).
  */
    u3_lua*
    u3_lua_boot(struct _u3_pier* pir_u);

  /* u3_lua_halt(): tear down a Lua state created by u3_lua_boot().
  */
    void
    u3_lua_halt(u3_lua* lua_u);

  /* u3_lua_push_noun(): push [som] onto the Lua stack as a noun userdata.
  **
  **   RETAIN: gains a reference owned by the Lua object; the caller keeps its
  **   own reference. The reference is released when Lua garbage-collects the
  **   userdata (__gc -> u3z).
  */
    void
    u3_lua_push_noun(lua_State* L, u3_noun som);

  /* u3_lua_get_noun(): read the noun userdata at stack index [idx].
  **
  **   RETAIN: returns a borrowed reference (still owned by the Lua object);
  **   u3k() it if you need to retain past the userdata's lifetime. Raises a
  **   Lua error if the value is not a noun userdata.
  */
    u3_noun
    u3_lua_get_noun(lua_State* L, c3_i idx);

  /* u3_lua_init(): load all Lua IO drivers from [pir_u]'s $pier/lua/ folder.
  **
  **   Boots a shared lua_State, scans the `<pax_c>/lua/` folder for `.lua`
  **   scripts, loads each script
  **   (which must `return` a driver table {name, priority, talk, kick, exit}),
  **   wraps each in a u3_auto, and returns them as one sub-chain sorted by
  **   ascending `priority` (lower = earlier = first crack at effects). The tail
  **   of the returned chain has nex_u == 0, ready to be spliced by the caller.
  **
  **   Returns 0 if the folder is empty/absent or no script loaded (and closes
  **   the otherwise-orphaned lua_State).
  */
    struct _u3_auto*
    u3_lua_init(struct _u3_pier* pir_u);

  /* u3_lua_load_dir(): u3_lua_init against an explicit directory.
  **
  **   The path-independent core of u3_lua_init; [pir_u] may be 0 (tests).
  */
    struct _u3_auto*
    u3_lua_load_dir(struct _u3_pier* pir_u, const c3_c* dir_c);

  /* u3_lua_attach(): load the Lua block after [anc_u] and watch for reloads.
  **
  **   Splices the loaded drivers into the live chain immediately after [anc_u]
  **   (typically `fore`), then starts a libuv watcher on $pier/lua/ so that
  **   adding/editing/removing a .lua file live-reloads the whole block. The
  **   pier talks the initial drivers itself; reloads talk the new ones.
  */
    void
    u3_lua_attach(struct _u3_pier* pir_u, struct _u3_auto* anc_u);

#endif /* U3_VERE_LUA_H */
