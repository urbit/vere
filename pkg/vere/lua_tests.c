/// @file
///
/// Unit tests for the Lua <-> noun bridge (lua.c).
///
/// Boots the noun system (loom) and a bridge lua_State, then exercises the
/// `noun` module from Lua and the C push/get API. Run via `zig build lua-test`.

#include "noun.h"
#include "vere.h"
#include "lua.h"

#include <string.h>
#include <unistd.h>

static u3_lua* lua_u;

/* _setup(): prepare the loom and a bridge state.
*/
static void
_setup(void)
{
  u3m_init(1 << 24);
  u3m_pave(c3y);

  lua_u = u3_lua_boot(0);
  if ( !lua_u ) {
    fprintf(stderr, "lua-test: boot failed\r\n");
    exit(1);
  }
}

/* _run(): execute a Lua chunk, fail the test on error.
*/
static void
_run(const char* nam_c, const char* src_c)
{
  lua_State* L = lua_u->lua;
  if ( LUA_OK != luaL_dostring(L, src_c) ) {
    fprintf(stderr, "lua-test: %s: %s\r\n", nam_c, lua_tostring(L, -1));
    exit(1);
  }
}

/* _test_build_inspect(): constructors + head/tail/to_int.
*/
static void
_test_build_inspect(void)
{
  _run("build_inspect",
    "local c = noun.cell(2, 3)                                   \n"
    "assert(noun.is_cell(c), 'cell is_cell')                     \n"
    "assert(not noun.is_atom(c), 'cell not is_atom')             \n"
    "assert(noun.to_int(noun.head(c)) == 2, 'head==2')           \n"
    "assert(noun.to_int(noun.tail(c)) == 3, 'tail==3')           \n"
    "local a = noun.atom(42)                                     \n"
    "assert(noun.is_atom(a), 'atom is_atom')                     \n"
    "assert(noun.to_int(a) == 42, 'atom==42')                    \n");
}

/* _test_cord(): cord round-trips through to_string.
*/
static void
_test_cord(void)
{
  _run("cord",
    "local cd = noun.cord('behn')                                \n"
    "assert(noun.is_atom(cd), 'cord is_atom')                    \n"
    "assert(noun.to_string(cd) == 'behn', 'cord roundtrip')      \n"
    "assert(noun.eq(noun.cord('ames'), noun.cord('ames')), 'eq') \n");
}

/* _test_equality(): structural equality of nested cells.
*/
static void
_test_equality(void)
{
  _run("equality",
    "assert(noun.eq(noun.cell(1,2), noun.cell(1,2)), 'eq cells') \n"
    "assert(not noun.eq(noun.cell(1,2), noun.cell(1,3)), 'neq')  \n"
    "assert(noun.cell(1,2) == noun.cell(1,2), '__eq metamethod') \n");
}

/* _test_list(): list shape [1 2 3 0].
*/
static void
_test_list(void)
{
  _run("list",
    "local l = noun.list(1, 2, 3)                                \n"
    "assert(noun.to_int(noun.head(l)) == 1, 'l.0==1')            \n"
    "local t = noun.tail(l)                                      \n"
    "assert(noun.to_int(noun.head(t)) == 2, 'l.1==2')            \n"
    "assert(noun.to_int(noun.head(noun.tail(t))) == 3, 'l.2==3') \n"
    "assert(noun.to_int(noun.tail(noun.tail(t))) == 0, 'nul')    \n");
}

/* _test_c_api(): push from C, read back inside Lua; get from C.
*/
static void
_test_c_api(void)
{
  lua_State* L = lua_u->lua;

  //  push a noun built in C, hand it to a Lua global, inspect it
  //
  u3_noun som = u3nc(u3i_word(7), u3i_word(8));      //  [7 8]
  u3_lua_push_noun(L, som);                          //  RETAIN: refcount now 2
  lua_setglobal(L, "from_c");
  u3z(som);                                          //  drop our ref -> 1

  _run("c_api",
    "assert(noun.is_cell(from_c), 'from_c cell')                 \n"
    "assert(noun.to_int(noun.head(from_c)) == 7, 'from_c.0==7')  \n"
    "assert(noun.to_int(noun.tail(from_c)) == 8, 'from_c.1==8')  \n");

  //  read it back into C and confirm value
  //
  lua_getglobal(L, "from_c");
  {
    u3_noun got = u3_lua_get_noun(L, -1);            //  borrowed
    if ( c3n == u3r_sing(got, u3nc(u3i_word(7), u3i_word(8))) ) {
      fprintf(stderr, "lua-test: c_api: get mismatch\r\n");
      exit(1);
    }
  }
  lua_pop(L, 1);
}

/* _test_errors(): bad uses raise Lua errors rather than crashing.
*/
static void
_test_errors(void)
{
  lua_State* L = lua_u->lua;

  //  head of an atom must error (pcall returns false)
  //
  if ( LUA_OK == luaL_dostring(L, "noun.head(noun.atom(5))") ) {
    fprintf(stderr, "lua-test: errors: head(atom) did not error\r\n");
    exit(1);
  }
  lua_pop(L, 1);

  //  negative atom must error
  //
  if ( LUA_OK == luaL_dostring(L, "noun.atom(-1)") ) {
    fprintf(stderr, "lua-test: errors: atom(-1) did not error\r\n");
    exit(1);
  }
  lua_pop(L, 1);
}

/* _test_gc(): forcing a full GC runs __gc on dropped nouns without crashing.
*/
static void
_test_gc(void)
{
  lua_State* L = lua_u->lua;
  _run("gc_churn",
    "for i = 1, 10000 do local x = noun.cell(noun.cord('spam'), i) end \n");
  lua_gc(L, LUA_GCCOLLECT, 0);
  lua_gc(L, LUA_GCCOLLECT, 0);
}

/*  ------------------------------------------------------------------------
 *  loader / meta-driver tests (step 03)
 *  ------------------------------------------------------------------------ */

/* _write(): write [body_c] to <dir_c>/<nam_c>.
*/
static void
_write(const char* dir_c, const char* nam_c, const char* body_c)
{
  char  pax_c[2048];
  snprintf(pax_c, sizeof(pax_c), "%s/%s", dir_c, nam_c);
  FILE* fil_u = fopen(pax_c, "wb");
  if ( !fil_u ) {
    fprintf(stderr, "lua-test: cannot write %s\r\n", pax_c);
    exit(1);
  }
  fwrite(body_c, 1, strlen(body_c), fil_u);
  fclose(fil_u);
}

/* _mktmp(): make a fresh temp directory, returning its path (static buffer).
*/
static char*
_mktmp(void)
{
  static char tmp_c[1024];
  snprintf(tmp_c, sizeof(tmp_c), "/tmp/vere-lua-test-XXXXXX");
  if ( !mkdtemp(tmp_c) ) {
    fprintf(stderr, "lua-test: mkdtemp failed\r\n");
    exit(1);
  }
  return tmp_c;
}

/* _chain_len(): number of drivers in a chain.
*/
static c3_w
_chain_len(u3_auto* car_u)
{
  c3_w len_w = 0;
  while ( car_u ) { len_w++; car_u = car_u->nex_u; }
  return len_w;
}

/* _test_loader_empty(): a folder with no scripts yields no drivers.
*/
static void
_test_loader_empty(void)
{
  char*    dir_c = _mktmp();
  u3_auto* car_u = u3_lua_load_dir(0, dir_c);
  if ( 0 != car_u ) {
    fprintf(stderr, "lua-test: empty dir should yield 0 drivers\r\n");
    exit(1);
  }
}

/* _test_loader_priority(): drivers load and sort by ascending priority.
*/
static void
_test_loader_priority(void)
{
  char* dir_c = _mktmp();

  //  intentionally out of order on disk; loader must sort by priority
  //
  _write(dir_c, "b.lua",
    "return { name = 'bee', priority = 20,"
    " kick = function(ctx, w, c) return false end }\n");
  _write(dir_c, "a.lua",
    "return { name = 'ayy', priority = 10,"
    " kick = function(ctx, w, c) return noun.eq(noun.head(w), noun.cord('ayy')) end }\n");
  _write(dir_c, "c.lua",
    "return { name = 'cee', priority = 30,"
    " talk = function(ctx) ctx:log('cee talked') end,"
    " kick = function(ctx, w, c) return false end }\n");
  _write(dir_c, "notme.txt", "ignored\n");

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u ) {
    fprintf(stderr, "lua-test: loader produced no drivers\r\n");
    exit(1);
  }
  if ( 3 != _chain_len(hed_u) ) {
    fprintf(stderr, "lua-test: expected 3 drivers, got %u\r\n",
            _chain_len(hed_u));
    exit(1);
  }

  //  all wrappers carry the c3__lua name mote
  //
  for ( u3_auto* c = hed_u; c; c = c->nex_u ) {
    if ( c3__lua != c->nam_m ) {
      fprintf(stderr, "lua-test: driver nam_m not c3__lua\r\n");
      exit(1);
    }
    u3_assert( c->io.talk_f && c->io.kick_f && c->io.exit_f );
  }

  //  start the drivers (runs talk, marks live)
  //
  u3_auto_talk(hed_u);
  if ( c3y != hed_u->liv_o ) {
    fprintf(stderr, "lua-test: talk did not mark live\r\n");
    exit(1);
  }

  //  the highest-priority (lowest number) driver is 'ayy'; its kick matches
  //  a wire whose head is 'ayy' and rejects others.
  //
  {
    u3_noun wir = u3nc(u3i_string("ayy"), u3_nul);
    u3_noun cad = u3nc(u3i_string("poke"), u3_nul);
    c3_o    ret = hed_u->io.kick_f(hed_u, u3k(wir), u3k(cad));
    if ( c3y != ret ) {
      fprintf(stderr, "lua-test: head driver should handle 'ayy' wire\r\n");
      exit(1);
    }

    u3_noun wir2 = u3nc(u3i_string("nope"), u3_nul);
    c3_o    ret2 = hed_u->io.kick_f(hed_u, u3k(wir2), u3k(cad));
    if ( c3n != ret2 ) {
      fprintf(stderr, "lua-test: head driver should reject 'nope' wire\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad); u3z(wir2);
  }

  //  teardown runs each exit and closes the shared state (no crash/leak)
  //
  u3_auto_exit(hed_u);
}

/* _test_loader_bad(): a script that doesn't return a table is skipped, not fatal.
*/
static void
_test_loader_bad(void)
{
  char* dir_c = _mktmp();
  _write(dir_c, "ok.lua",
    "return { name = 'ok', priority = 5,"
    " kick = function(ctx,w,c) return false end }\n");
  _write(dir_c, "bad.lua", "return 42\n");           //  not a table
  _write(dir_c, "boom.lua", "error('nope')\n");      //  raises on load

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u || 1 != _chain_len(hed_u) ) {
    fprintf(stderr, "lua-test: expected exactly 1 good driver\r\n");
    exit(1);
  }
  u3_auto_exit(hed_u);
}

/* _test_ctx_guards(): plan/after/every are registered and fail cleanly with no
**   pier (the test context), without crashing the runtime.
*/
static void
_test_ctx_guards(void)
{
  char* dir_c = _mktmp();
  _write(dir_c, "io.lua",
    "return { name = 'io', priority = 1,                                  \n"
    "  talk = function(ctx) ctx:after(5, function() end) end,             \n"
    "  kick = function(ctx, w, c)                                         \n"
    "    ctx:plan('g', noun.list('wire'), noun.cell(noun.cord('poke'), 0))\n"
    "    return true                                                      \n"
    "  end }                                                              \n");

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u ) {
    fprintf(stderr, "lua-test: ctx driver failed to load\r\n");
    exit(1);
  }

  //  talk calls ctx:after with no pier -> raises inside, caught & logged
  //
  u3_auto_talk(hed_u);

  //  kick calls ctx:plan with no pier -> raises inside -> kick returns c3n
  //
  {
    u3_noun wir = u3nc(u3i_string("wire"), u3_nul);
    u3_noun cad = u3nc(u3i_string("poke"), u3_nul);
    c3_o    ret = hed_u->io.kick_f(hed_u, u3k(wir), u3k(cad));
    if ( c3n != ret ) {
      fprintf(stderr, "lua-test: kick should fail without a pier\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad);
  }

  //  no timers should remain registered (after() raised before starting one)
  //
  u3_auto_exit(hed_u);
}

/* _test_socket_guards(): udp_open/tcp_listen/tcp_connect exist and fail cleanly
**   with no pier (no libuv loop) instead of crashing.
*/
static void
_test_socket_guards(void)
{
  char* dir_c = _mktmp();
  _write(dir_c, "s.lua",
    "return { name='s', priority=1,                                          \n"
    "  kick = function(ctx, w, c)                                            \n"
    "    local ok1 = pcall(function() ctx:udp_open(40000) end)              \n"
    "    local ok2 = pcall(function() ctx:tcp_listen(40001, function() end) end)\n"
    "    local ok3 = pcall(function() ctx:tcp_connect('127.0.0.1', 40002, function() end) end)\n"
    "    return (not ok1) and (not ok2) and (not ok3)                        \n"
    "  end }                                                                 \n");

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u ) {
    fprintf(stderr, "lua-test: socket driver failed to load\r\n");
    exit(1);
  }

  //  kick returns true only if all three socket calls raised (no pier)
  //
  {
    u3_noun wir = u3nc(u3i_string("w"), u3_nul);
    u3_noun cad = u3nc(u3i_string("c"), u3_nul);
    if ( c3y != hed_u->io.kick_f(hed_u, u3k(wir), u3k(cad)) ) {
      fprintf(stderr, "lua-test: socket calls should raise without a pier\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad);
  }

  u3_auto_exit(hed_u);
}

/* _test_isolation(): each driver has its own globals (separate lua_State).
*/
static void
_test_isolation(void)
{
  char* dir_c = _mktmp();

  //  both set the global MARK; with isolation each driver sees only its own
  //
  _write(dir_c, "a.lua",
    "MARK = 'a'\n"
    "return { name='a', priority=1,"
    " kick=function(ctx,w,c) return MARK == 'a' end }\n");
  _write(dir_c, "b.lua",
    "MARK = 'b'\n"
    "return { name='b', priority=2,"
    " kick=function(ctx,w,c) return MARK == 'b' end }\n");

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u || 2 != _chain_len(hed_u) ) {
    fprintf(stderr, "lua-test: isolation: expected 2 drivers\r\n");
    exit(1);
  }

  //  every driver's own-MARK check must pass: shared state would clobber one
  //
  for ( u3_auto* c = hed_u; c; c = c->nex_u ) {
    u3_noun wir = u3nc(u3i_string("w"), u3_nul);
    u3_noun cad = u3nc(u3i_string("c"), u3_nul);
    if ( c3y != c->io.kick_f(c, u3k(wir), u3k(cad)) ) {
      fprintf(stderr, "lua-test: isolation: a driver saw a foreign MARK\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad);
  }

  u3_auto_exit(hed_u);
}

/* _test_fs(): pier-scoped filesystem ops, against a fake pier root.
*/
static void
_test_fs(void)
{
  char*   root_c = _mktmp();                 //  stands in for the pier root
  char*   dir_c  = _mktmp();                 //  the driver folder
  u3_pier fak_u;
  memset(&fak_u, 0, sizeof(fak_u));
  fak_u.pax_c = root_c;

  _write(dir_c, "fs.lua",
    "return { name='fs', priority=1, kick=function(ctx, w, c)            \n"
    "  ctx:write('hello.txt', 'world')                                  \n"
    "  assert(ctx:read('hello.txt') == 'world', 'read back')            \n"
    "  assert(ctx:exists('hello.txt'), 'exists')                        \n"
    "  ctx:mkdir('sub')                                                 \n"
    "  local st = ctx:stat('sub'); assert(st and st.kind=='dir', 'dir') \n"
    "  ctx:write('sub/inner.txt', 'deep')                               \n"
    "  assert(ctx:read('sub/inner.txt') == 'deep', 'nested')           \n"
    "  local found = false                                              \n"
    "  for _, n in ipairs(ctx:list('.')) do                             \n"
    "    if n == 'hello.txt' then found = true end                      \n"
    "  end                                                              \n"
    "  assert(found, 'list')                                            \n"
    "  assert(not pcall(function() ctx:read('../escape') end), 'escape')\n"
    "  assert(not pcall(function() ctx:read('/etc/passwd') end), 'abs') \n"
    "  ctx:remove('hello.txt')                                          \n"
    "  assert(not ctx:exists('hello.txt'), 'removed')                   \n"
    "  assert(#ctx:pier_path() > 0, 'pier_path')                        \n"
    "  return true                                                      \n"
    "end }                                                              \n");

  u3_auto* hed_u = u3_lua_load_dir((struct _u3_pier*)&fak_u, dir_c);
  if ( !hed_u ) {
    fprintf(stderr, "lua-test: fs driver failed to load\r\n");
    exit(1);
  }

  {
    u3_noun wir = u3nc(u3i_string("w"), u3_nul);
    u3_noun cad = u3nc(u3i_string("c"), u3_nul);
    if ( c3y != hed_u->io.kick_f(hed_u, u3k(wir), u3k(cad)) ) {
      fprintf(stderr, "lua-test: fs operations failed\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad);
  }

  u3_auto_exit(hed_u);
}

/* _test_io2_guards(): the second wave of ctx IO (scry/http/pipe/watch/async fs)
**   is registered and fails cleanly with no pier.
*/
static void
_test_io2_guards(void)
{
  char* dir_c = _mktmp();
  _write(dir_c, "g.lua",
    "return { name='g', priority=1, kick=function(ctx, w, c)              \n"
    "  local checks = {                                                  \n"
    "    function() ctx:scry('cy','base','/', function() end) end,       \n"
    "    function() ctx:http('GET','http://x/', nil, function() end) end, \n"
    "    function() ctx:pipe_connect('/tmp/x.sock', function() end) end,  \n"
    "    function() ctx:pipe_listen('/tmp/x.sock', function() end) end,   \n"
    "    function() ctx:watch('f', function() end) end,                   \n"
    "    function() ctx:read_async('f', function() end) end,              \n"
    "    function() ctx:write_async('f', 'd', function() end) end,        \n"
    "  }                                                                  \n"
    "  for _, fn in ipairs(checks) do                                     \n"
    "    if pcall(fn) then return false end  -- must raise w/o a pier     \n"
    "  end                                                                \n"
    "  return true                                                        \n"
    "end }                                                                \n");

  u3_auto* hed_u = u3_lua_load_dir(0, dir_c);
  if ( !hed_u ) {
    fprintf(stderr, "lua-test: io2 driver failed to load\r\n");
    exit(1);
  }
  {
    u3_noun wir = u3nc(u3i_string("w"), u3_nul);
    u3_noun cad = u3nc(u3i_string("c"), u3_nul);
    if ( c3y != hed_u->io.kick_f(hed_u, u3k(wir), u3k(cad)) ) {
      fprintf(stderr, "lua-test: io2 methods should raise without a pier\r\n");
      exit(1);
    }
    u3z(wir); u3z(cad);
  }
  u3_auto_exit(hed_u);
}

int
main(int argc, char* argv[])
{
  _setup();

  _test_build_inspect();
  _test_cord();
  _test_equality();
  _test_list();
  _test_c_api();
  _test_errors();
  _test_gc();

  u3_lua_halt(lua_u);

  _test_loader_empty();
  _test_loader_priority();
  _test_loader_bad();
  _test_ctx_guards();
  _test_isolation();
  _test_socket_guards();
  _test_fs();
  _test_io2_guards();

  fprintf(stderr, "test_lua: ok\n");
  return 0;
}
