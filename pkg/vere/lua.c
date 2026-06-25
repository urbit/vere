/// @file
///
/// Lua <-> noun bridge for vere. See lua.h for the public surface.
///
/// This translation unit owns:
///   - the "u3.noun" userdata type that wraps a u3_noun on the Lua side, with
///     a __gc metamethod that releases the king's reference (u3z), keeping
///     Lua's garbage collector and the noun refcounter in sync;
///   - the global `noun` module, exposing constructors/inspectors to scripts;
///   - boot/halt of a sandboxed lua_State.
///
/// Reference-counting discipline (the sharp edge of this file):
///   - A noun userdata *owns exactly one* reference to its noun, released in
///     _noun_gc.
///   - _push_owned() takes ownership of a freshly-built noun (no u3k).
///   - u3_lua_push_noun() RETAINS: it u3k()s, then _push_owned()s the copy.
///   - _check_noun() returns a *borrowed* reference (do not u3z).
///   - _arg_noun() returns an *owned* reference (+1): either a fresh noun
///     (from an int/string) or u3k() of a userdata's noun. It is meant to be
///     handed straight to a consuming constructor (u3i_cell, ...).

#include "vere.h"
#include "noun.h"
#include "lua.h"

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define U3_LUA_NOUN  "u3.noun"          //  metatable name for noun userdata
#define U3_LUA_CTX   "u3.ctx"           //  metatable name for driver context

/* u3_lua_driver: a u3_auto driver backed by a Lua table.
**
**   The embedded u3_auto must be the first field so a u3_auto* can be cast to
**   u3_lua_driver*. [tab_r]/[ctx_r] are luaL_ref handles into the registry for
**   the driver table and its ctx userdata; [lua_u] is the shared, ref-counted
**   bridge state.
*/
  struct _u3_lua_timer;
  struct _u3_lua_io;

  typedef struct _u3_lua_driver {
    u3_auto  car_u;                     //  driver (first field)
    u3_lua*  lua_u;                     //  this driver's isolated state
    c3_w     pri_w;                     //  priority (lower = earlier)
    c3_c*    nam_c;                     //  driver name (for logging)
    int      tab_r;                     //  registry ref: driver table
    int      ctx_r;                     //  registry ref: ctx userdata
    struct _u3_lua_timer* tim_u;        //  live libuv timers (linked list)
    struct _u3_lua_io*    io_u;         //  live libuv sockets (linked list)
  } u3_lua_driver;

  /* u3_lua_timer: a libuv timer owned by a Lua driver.
  **
  **   The embedded uv_timer_t must outlive the driver wrapper: uv_close() is
  **   async, so the handle is freed in its close callback, which touches only
  **   the timer struct (never the lua_State or the driver, which may be gone).
  */
  typedef struct _u3_lua_timer {
    uv_timer_t            tim_u;        //  libuv handle (.data -> this)
    u3_lua_driver*        dvr_u;        //  owning driver (borrowed)
    int                   fn_r;         //  registry ref: callback function
    c3_o                  rep_o;        //  c3y if repeating
    struct _u3_lua_timer* nex_u;        //  next in driver's list
  } u3_lua_timer;

  /* u3_lua_io: a libuv socket (UDP, TCP connection, or TCP listener) owned by
  **   a Lua driver. Same async-close discipline as timers: uv_close is
  **   deferred, the close callback frees the struct and touches nothing else.
  **
  **   The socket is owned by the *driver*, never by Lua's GC. The Lua-facing
  **   "u3.io" userdata is a weak handle validated against the driver's live
  **   list (by pointer + [uid_d], ABA-safe) before every use, so a method on a
  **   closed socket raises rather than dereferencing freed memory. This is safe
  **   because Lua callbacks only run while the driver (hence its state) lives.
  */
  typedef struct _u3_lua_io {
    union {                            //  libuv handle (.data -> this)
      uv_udp_t            udp_u;
      uv_tcp_t            tcp_u;
      uv_stream_t         str_u;
      uv_handle_t         han_u;
    };
    c3_y                  typ_y;       //  'u' udp, 'c' tcp conn, 'l' tcp listen
    u3_lua_driver*        dvr_u;       //  owning driver (borrowed)
    int                   cb_r;        //  recv/accept callback (LUA_NOREF if 0)
    c3_d                  uid_d;       //  unique id (ABA-safe validation)
    c3_o                  dead_o;      //  closing/closed
    struct _u3_lua_io*    nex_u;       //  next in driver's list
  } u3_lua_io;

  /* u3_lua_wreq: an outstanding UDP/TCP write, carrying its own payload copy.
  */
  typedef struct _u3_lua_wreq {
    union {
      uv_udp_send_t       udp_u;
      uv_write_t          tcp_u;
    };
    c3_c*                 nam_c;       //  driver name (for error logs)
    c3_y                  dat_y[];     //  payload (flexible array)
  } u3_lua_wreq;

//  small helpers defined later in this file
//
static c3_c* _lua_strdup(const c3_c* str_c);
static c3_c* _lua_join(const c3_c* dir_c, const c3_c* nam_c);
static c3_y* _lua_slurp(const c3_c* pax_c, size_t* len_i);

/* _push_owned(): wrap [som] as a noun userdata, taking ownership of its ref.
*/
static void
_push_owned(lua_State* L, u3_noun som)
{
  u3_noun* box = (u3_noun*)lua_newuserdatauv(L, sizeof(u3_noun), 0);
  *box = som;
  luaL_setmetatable(L, U3_LUA_NOUN);
}

/* _check_noun(): borrow the noun in the userdata at [idx]. RETAIN
*/
static u3_noun
_check_noun(lua_State* L, int idx)
{
  u3_noun* box = (u3_noun*)luaL_checkudata(L, idx, U3_LUA_NOUN);
  return *box;
}

/* _arg_noun(): coerce the value at [idx] to an owned noun (+1 ref).
**
**   Accepts a noun userdata (gains a ref), a Lua integer (-> atom), or a Lua
**   string (-> LSB-first byte atom / cord). Result is owned by the caller and
**   is normally consumed by a constructor such as u3i_cell.
*/
static u3_noun
_arg_noun(lua_State* L, int idx)
{
  switch ( lua_type(L, idx) ) {
    case LUA_TNUMBER: {
      lua_Integer n = luaL_checkinteger(L, idx);
      if ( n < 0 ) {
        luaL_error(L, "noun: negative atom (%lld)", (long long)n);
      }
      return u3i_chub((c3_d)n);
    }

    case LUA_TSTRING: {
      size_t      len_i;
      const char* str_c = lua_tolstring(L, idx, &len_i);
      return u3i_bytes((c3_w)len_i, (const c3_y*)str_c);
    }

    default: {
      //  must be a noun userdata; raises if not
      //
      return u3k(_check_noun(L, idx));
    }
  }
}

/*  ------------------------------------------------------------------------
 *  metatable methods
 *  ------------------------------------------------------------------------ */

/* _noun_gc(): release the noun reference owned by a collected userdata.
*/
static int
_noun_gc(lua_State* L)
{
  u3_noun* box = (u3_noun*)luaL_checkudata(L, 1, U3_LUA_NOUN);
  if ( u3_none != *box ) {
    u3z(*box);
    *box = u3_none;                     //  guard against double free
  }
  return 0;
}

/* _noun_tostring(): "<noun atom mug=...>" / "<noun cell mug=...>"
*/
static int
_noun_tostring(lua_State* L)
{
  u3_noun som = _check_noun(L, 1);
  lua_pushfstring(L, "<noun %s mug=%d>",
                  _(u3a_is_cell(som)) ? "cell" : "atom",
                  (int)u3r_mug(som));
  return 1;
}

/* _noun_eq_mt(): __eq -> noun value equality.
*/
static int
_noun_eq_mt(lua_State* L)
{
  u3_noun a = _check_noun(L, 1);
  u3_noun b = _check_noun(L, 2);
  lua_pushboolean(L, _(u3r_sing(a, b)));
  return 1;
}

/*  ------------------------------------------------------------------------
 *  `noun` module: constructors
 *  ------------------------------------------------------------------------ */

/* noun.atom(int): a direct/indirect atom from a non-negative Lua integer.
*/
static int
_noun_atom(lua_State* L)
{
  lua_Integer n = luaL_checkinteger(L, 1);
  if ( n < 0 ) {
    return luaL_error(L, "noun.atom: negative (%lld)", (long long)n);
  }
  _push_owned(L, u3i_chub((c3_d)n));
  return 1;
}

/* noun.cord(str): an LSB-first byte atom (a @t cord) from a Lua string.
*/
static int
_noun_cord(lua_State* L)
{
  size_t      len_i;
  const char* str_c = luaL_checklstring(L, 1, &len_i);
  _push_owned(L, u3i_bytes((c3_w)len_i, (const c3_y*)str_c));
  return 1;
}

/* noun.tape(str): a tape (list of byte atoms) from a Lua string.
*/
static int
_noun_tape(lua_State* L)
{
  const char* str_c = luaL_checkstring(L, 1);
  _push_owned(L, u3i_tape(str_c));
  return 1;
}

/* noun.cell(a, b): the cell [a b]. Args are coerced via _arg_noun.
*/
static int
_noun_cell(lua_State* L)
{
  u3_noun a = _arg_noun(L, 1);
  u3_noun b = _arg_noun(L, 2);
  _push_owned(L, u3i_cell(a, b));        //  consumes a, b
  return 1;
}

/* noun.list(...): a null-terminated list from the arguments.
*/
static int
_noun_list(lua_State* L)
{
  int     top_i = lua_gettop(L);
  u3_noun lis   = u3_nul;

  for ( int i = top_i; i >= 1; i-- ) {
    u3_noun hed = _arg_noun(L, i);       //  owned (+1)
    lis = u3i_cell(hed, lis);            //  consumes hed and prior lis
  }
  _push_owned(L, lis);
  return 1;
}

/*  ------------------------------------------------------------------------
 *  `noun` module: inspectors
 *  ------------------------------------------------------------------------ */

/* noun.is_atom(n) / noun.is_cell(n)
*/
static int
_noun_is_atom(lua_State* L)
{
  lua_pushboolean(L, _(u3a_is_atom(_check_noun(L, 1))));
  return 1;
}

static int
_noun_is_cell(lua_State* L)
{
  lua_pushboolean(L, _(u3a_is_cell(_check_noun(L, 1))));
  return 1;
}

/* noun.head(n) / noun.tail(n): the head/tail of a cell (errors on an atom).
*/
static int
_noun_head(lua_State* L)
{
  u3_noun som = _check_noun(L, 1);
  if ( c3n == u3a_is_cell(som) ) {
    return luaL_error(L, "noun.head: not a cell");
  }
  u3_lua_push_noun(L, u3h(som));         //  RETAIN (gains)
  return 1;
}

static int
_noun_tail(lua_State* L)
{
  u3_noun som = _check_noun(L, 1);
  if ( c3n == u3a_is_cell(som) ) {
    return luaL_error(L, "noun.tail: not a cell");
  }
  u3_lua_push_noun(L, u3t(som));         //  RETAIN (gains)
  return 1;
}

/* noun.eq(a, b): noun value equality.
*/
static int
_noun_eq(lua_State* L)
{
  lua_pushboolean(L, _(u3r_sing(_check_noun(L, 1), _check_noun(L, 2))));
  return 1;
}

/* noun.mug(n): the 31-bit mug of a noun, as a Lua integer.
*/
static int
_noun_mug(lua_State* L)
{
  lua_pushinteger(L, (lua_Integer)u3r_mug(_check_noun(L, 1)));
  return 1;
}

/* noun.to_int(n): an atom as a Lua integer, or nil if it exceeds 64 bits.
*/
static int
_noun_to_int(lua_State* L)
{
  u3_noun som = _check_noun(L, 1);
  if ( c3n == u3a_is_atom(som) ) {
    return luaL_error(L, "noun.to_int: not an atom");
  }
  if ( u3r_met(6, som) > 1 ) {           //  more than one 64-bit chunk
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, (lua_Integer)u3r_chub(0, som));
  return 1;
}

/* noun.to_string(n): a cord atom as a Lua string.
*/
static int
_noun_to_string(lua_State* L)
{
  u3_noun som = _check_noun(L, 1);
  if ( c3n == u3a_is_atom(som) ) {
    return luaL_error(L, "noun.to_string: not an atom");
  }
  {
    c3_c* str_c = u3r_string(som);       //  malloced
    lua_pushstring(L, str_c);
    c3_free(str_c);
  }
  return 1;
}

/*  ------------------------------------------------------------------------
 *  registration / boot
 *  ------------------------------------------------------------------------ */

static const luaL_Reg _noun_funcs[] = {
  { "atom",      _noun_atom      },
  { "cord",      _noun_cord      },
  { "tape",      _noun_tape      },
  { "cell",      _noun_cell      },
  { "list",      _noun_list      },
  { "is_atom",   _noun_is_atom   },
  { "is_cell",   _noun_is_cell   },
  { "head",      _noun_head      },
  { "tail",      _noun_tail      },
  { "eq",        _noun_eq        },
  { "mug",       _noun_mug       },
  { "to_int",    _noun_to_int    },
  { "to_string", _noun_to_string },
  { 0, 0 }
};

/*  ------------------------------------------------------------------------
 *  driver context (`ctx`) -- the handle a Lua driver uses to talk back
 *
 *  In this step ctx exposes only :log(). Step 05 adds :plan() (inject events
 *  into Arvo) and libuv-backed real I/O on this same object.
 *  ------------------------------------------------------------------------ */

/* _check_ctx(): the driver behind a ctx userdata at [idx].
*/
static u3_lua_driver*
_check_ctx(lua_State* L, int idx)
{
  return *(u3_lua_driver**)luaL_checkudata(L, idx, U3_LUA_CTX);
}

/* ctx:log(str): write a line to the runtime log, tagged with the driver name.
*/
static int
_ctx_log(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const char*    msg_c = luaL_checkstring(L, 2);
  u3l_log("lua: %s: %s", dvr_u->nam_c ? dvr_u->nam_c : "?", msg_c);
  return 0;
}

/* ctx:plan(vane, wire, card): inject an event into Arvo.
**
**   [vane] is a short string naming the target (e.g. "b" behn, "g" gall),
**   becoming the first knot of the wire. [wire] must be a path (a non-empty
**   cell); [card] is the [%tag data] effect. This is the bridge from the
**   outside world into the running ship.
*/
static int
_ctx_plan(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);

  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:plan: unavailable without a pier");
  }

  size_t      vln_i;
  const char* van_c = luaL_checklstring(L, 2, &vln_i);
  u3_noun     tar   = u3i_bytes((c3_w)vln_i, (const c3_y*)van_c);
  u3_noun     wir   = _arg_noun(L, 3);                //  owned
  u3_noun     cad   = _arg_noun(L, 4);                //  owned

  if ( c3n == u3a_is_cell(wir) ) {
    u3z(tar); u3z(wir); u3z(cad);
    return luaL_error(L, "ctx:plan: wire must be a non-empty path");
  }

  //  u3_ovum_init consumes tar/wir/cad
  //
  u3_auto_plan(&dvr_u->car_u, u3_ovum_init(0, tar, wir, cad));
  return 0;
}

/* ctx:wish(str): evaluate a hoon expression king-side (ivory kernel).
**
**   Returns the resulting noun. Limited to what the ivory pill supports (text
**   parsing, scot/scow, arithmetic, ...) -- not the full ship kernel.
*/
static int
_ctx_wish(lua_State* L)
{
  (void)_check_ctx(L, 1);
  const char* exp_c = luaL_checkstring(L, 2);
  _push_owned(L, u3v_wish(exp_c));                    //  transfers ownership
  return 1;
}

/* _lua_timer_close_cb(): free a timer handle once libuv has closed it.
**
**   Runs on a later loop tick; must not touch the lua_State or the driver,
**   either of which may already be gone.
*/
static void
_lua_timer_close_cb(uv_handle_t* han_u)
{
  c3_free(han_u->data);
}

/* _lua_timer_unlink(): remove [tim_u] from its driver's timer list.
*/
static void
_lua_timer_unlink(u3_lua_driver* dvr_u, u3_lua_timer* tim_u)
{
  u3_lua_timer** pre_u = &dvr_u->tim_u;
  while ( *pre_u ) {
    if ( *pre_u == tim_u ) {
      *pre_u = tim_u->nex_u;
      return;
    }
    pre_u = &(*pre_u)->nex_u;
  }
}

/* _lua_timer_cb(): fire a Lua timer callback as fn(ctx).
*/
static void
_lua_timer_cb(uv_timer_t* uvt_u)
{
  u3_lua_timer*  tim_u = uvt_u->data;
  u3_lua_driver* dvr_u = tim_u->dvr_u;
  lua_State*     L     = dvr_u->lua_u->lua;

  lua_rawgeti(L, LUA_REGISTRYINDEX, tim_u->fn_r);    //  callback
  lua_rawgeti(L, LUA_REGISTRYINDEX, dvr_u->ctx_r);   //  ctx
  if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
    u3l_log("lua: %s: timer error: %s",
            dvr_u->nam_c, lua_tostring(L, -1));
    lua_pop(L, 1);
  }

  //  a one-shot timer disposes itself after firing
  //
  if ( c3n == tim_u->rep_o ) {
    _lua_timer_unlink(dvr_u, tim_u);
    luaL_unref(L, LUA_REGISTRYINDEX, tim_u->fn_r);
    uv_close((uv_handle_t*)&tim_u->tim_u, _lua_timer_close_cb);
  }
}

/* _ctx_timer_start(): shared impl for ctx:after / ctx:every.
*/
static int
_ctx_timer_start(lua_State* L, c3_o rep_o)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);

  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx timer: unavailable without a pier");
  }

  lua_Integer ms_i = luaL_checkinteger(L, 2);
  if ( ms_i < 0 ) {
    return luaL_error(L, "ctx timer: negative interval");
  }
  luaL_checktype(L, 3, LUA_TFUNCTION);

  lua_pushvalue(L, 3);
  int fn_r = luaL_ref(L, LUA_REGISTRYINDEX);

  u3_lua_timer* tim_u = c3_calloc(sizeof(*tim_u));
  tim_u->dvr_u = dvr_u;
  tim_u->fn_r  = fn_r;
  tim_u->rep_o = rep_o;
  tim_u->nex_u = dvr_u->tim_u;
  dvr_u->tim_u = tim_u;

  uv_timer_init(u3L, &tim_u->tim_u);
  tim_u->tim_u.data = tim_u;
  uv_timer_start(&tim_u->tim_u, _lua_timer_cb,
                 (uint64_t)ms_i,
                 (c3y == rep_o) ? (uint64_t)ms_i : 0);
  return 0;
}

/* ctx:after(ms, fn): call fn(ctx) once after [ms] milliseconds.
*/
static int
_ctx_after(lua_State* L)
{
  return _ctx_timer_start(L, c3n);
}

/* ctx:every(ms, fn): call fn(ctx) every [ms] milliseconds.
*/
static int
_ctx_every(lua_State* L)
{
  return _ctx_timer_start(L, c3y);
}

/* _lua_close_timers(): stop and async-close all of a driver's timers.
**
**   Called from the driver's exit trampoline while the lua_State is still
**   open, so fn refs can be released here; the handles free themselves later.
*/
static void
_lua_close_timers(u3_lua_driver* dvr_u)
{
  lua_State*    L     = dvr_u->lua_u->lua;
  u3_lua_timer* tim_u = dvr_u->tim_u;

  while ( tim_u ) {
    u3_lua_timer* nex_u = tim_u->nex_u;
    uv_timer_stop(&tim_u->tim_u);
    luaL_unref(L, LUA_REGISTRYINDEX, tim_u->fn_r);
    uv_close((uv_handle_t*)&tim_u->tim_u, _lua_timer_close_cb);
    tim_u = nex_u;
  }
  dvr_u->tim_u = 0;
}

/*  ------------------------------------------------------------------------
 *  sockets: UDP + TCP, on the same driver-owned handle pattern as timers
 *  ------------------------------------------------------------------------ */

#define U3_LUA_IO  "u3.io"              //  metatable name for socket handles

static c3_d g_uid_d = 0;               //  monotonic id for ABA-safe handles

/* u3_lua_io_ref: the "u3.io" userdata -- a weak handle to a driver socket.
*/
typedef struct {
  u3_lua_driver* dvr_u;
  u3_lua_io*     io_u;
  c3_d           uid_d;
} u3_lua_io_ref;

/* _io_alloc_cb(): libuv read/recv buffer allocator (4K is ample).
*/
static void
_io_alloc_cb(uv_handle_t* han_u, size_t len_i, uv_buf_t* buf_u)
{
  buf_u->base = c3_malloc(4096);
  buf_u->len  = 4096;
}

/* _io_close_free_cb(): free a socket struct once libuv has closed it.
*/
static void
_io_close_free_cb(uv_handle_t* han_u)
{
  c3_free(han_u->data);
}

/* _io_unlink(): remove [io_u] from its driver's socket list.
*/
static void
_io_unlink(u3_lua_driver* dvr_u, u3_lua_io* io_u)
{
  u3_lua_io** pre_u = &dvr_u->io_u;
  while ( *pre_u ) {
    if ( *pre_u == io_u ) { *pre_u = io_u->nex_u; return; }
    pre_u = &(*pre_u)->nex_u;
  }
}

/* _io_kill(): close one socket -- unlink, release its callback, async-close.
*/
static void
_io_kill(u3_lua_io* io_u)
{
  if ( c3y == io_u->dead_o ) {
    return;
  }
  io_u->dead_o = c3y;
  {
    lua_State* L = io_u->dvr_u->lua_u->lua;
    if ( LUA_NOREF != io_u->cb_r ) {
      luaL_unref(L, LUA_REGISTRYINDEX, io_u->cb_r);
      io_u->cb_r = LUA_NOREF;
    }
  }
  _io_unlink(io_u->dvr_u, io_u);
  uv_close(&io_u->han_u, _io_close_free_cb);
}

/* _lua_close_io(): close every socket a driver owns (on exit).
*/
static void
_lua_close_io(u3_lua_driver* dvr_u)
{
  u3_lua_io* io_u = dvr_u->io_u;
  lua_State* L    = dvr_u->lua_u->lua;

  while ( io_u ) {
    u3_lua_io* nex_u = io_u->nex_u;
    io_u->dead_o = c3y;
    if ( LUA_NOREF != io_u->cb_r ) {
      luaL_unref(L, LUA_REGISTRYINDEX, io_u->cb_r);
    }
    uv_close(&io_u->han_u, _io_close_free_cb);
    io_u = nex_u;
  }
  dvr_u->io_u = 0;
}

/* _push_io(): push a "u3.io" weak handle for [io_u].
*/
static void
_push_io(lua_State* L, u3_lua_driver* dvr_u, u3_lua_io* io_u)
{
  u3_lua_io_ref* ref_u = lua_newuserdatauv(L, sizeof(*ref_u), 0);
  ref_u->dvr_u = dvr_u;
  ref_u->io_u  = io_u;
  ref_u->uid_d = io_u->uid_d;
  luaL_setmetatable(L, U3_LUA_IO);
}

/* _io_resolve(): the live socket behind a "u3.io" handle, or 0 if closed.
**
**   Validated against the driver's live list by pointer + uid (ABA-safe), so a
**   handle to a closed socket never dereferences freed memory.
*/
static u3_lua_io*
_io_resolve(lua_State* L, int idx)
{
  u3_lua_io_ref* ref_u = luaL_checkudata(L, idx, U3_LUA_IO);
  for ( u3_lua_io* p = ref_u->dvr_u->io_u; p; p = p->nex_u ) {
    if ( (p == ref_u->io_u) && (p->uid_d == ref_u->uid_d) && (c3n == p->dead_o) ) {
      return p;
    }
  }
  return 0;
}

/* _io_addr_name(): fill [hos_c]/[*por_i] from a sockaddr (ipv4).
*/
static void
_io_addr_name(const struct sockaddr* adr_u, c3_c* hos_c, c3_w len_w, c3_i* por_i)
{
  hos_c[0] = 0;
  *por_i   = 0;
  if ( adr_u && (AF_INET == adr_u->sa_family) ) {
    struct sockaddr_in* a_u = (struct sockaddr_in*)adr_u;
    uv_ip4_name(a_u, hos_c, len_w);
    *por_i = ntohs(a_u->sin_port);
  }
}

/*  --- UDP --- */

static void
_udp_recv_cb(uv_udp_t*              uvu_u,
             ssize_t               nrd_i,
             const uv_buf_t*       buf_u,
             const struct sockaddr* adr_u,
             unsigned              flg_i)
{
  u3_lua_io* io_u = uvu_u->data;

  if ( (nrd_i > 0) && adr_u && (LUA_NOREF != io_u->cb_r) ) {
    lua_State* L = io_u->dvr_u->lua_u->lua;
    c3_c       hos_c[64];
    c3_i       por_i;
    _io_addr_name(adr_u, hos_c, sizeof(hos_c), &por_i);

    lua_rawgeti(L, LUA_REGISTRYINDEX, io_u->cb_r);   //  handler
    lua_pushlstring(L, buf_u->base, (size_t)nrd_i);  //  data
    lua_pushstring(L, hos_c);                        //  host
    lua_pushinteger(L, por_i);                       //  port
    if ( LUA_OK != lua_pcall(L, 3, 0, 0) ) {
      u3l_log("lua: %s: udp recv error: %s",
              io_u->dvr_u->nam_c, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  if ( buf_u->base ) {
    c3_free(buf_u->base);
  }
}

static void
_udp_send_cb(uv_udp_send_t* req_u, c3_i sas_i)
{
  u3_lua_wreq* wre_u = req_u->data;
  if ( sas_i < 0 ) {
    u3l_log("lua: %s: udp send failed: %s", wre_u->nam_c, uv_strerror(sas_i));
  }
  c3_free(wre_u);
}

/* ctx:udp_open(port[, fn]): bind a UDP socket on [port]; fn(data, host, port).
*/
static int
_ctx_udp_open(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:udp_open: unavailable without a pier");
  }

  lua_Integer por_i  = luaL_checkinteger(L, 2);
  c3_o        haf_o  = ( (lua_gettop(L) >= 3) && !lua_isnil(L, 3) ) ? c3y : c3n;
  if ( c3y == haf_o ) {
    luaL_checktype(L, 3, LUA_TFUNCTION);
  }

  u3_lua_io* io_u = c3_calloc(sizeof(*io_u));
  io_u->typ_y = 'u';
  io_u->dvr_u = dvr_u;
  io_u->cb_r  = LUA_NOREF;
  io_u->uid_d = ++g_uid_d;
  io_u->dead_o = c3n;
  uv_udp_init(u3L, &io_u->udp_u);
  io_u->udp_u.data = io_u;

  {
    struct sockaddr_in adr_u;
    uv_ip4_addr("0.0.0.0", (c3_i)por_i, &adr_u);
    c3_i r_i = uv_udp_bind(&io_u->udp_u, (const struct sockaddr*)&adr_u,
                           UV_UDP_REUSEADDR);
    if ( r_i ) {
      uv_close(&io_u->han_u, _io_close_free_cb);
      return luaL_error(L, "ctx:udp_open: bind :%d: %s",
                        (c3_i)por_i, uv_strerror(r_i));
    }
  }

  if ( c3y == haf_o ) {
    lua_pushvalue(L, 3);
    io_u->cb_r = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  io_u->nex_u  = dvr_u->io_u;
  dvr_u->io_u  = io_u;
  uv_udp_recv_start(&io_u->udp_u, _io_alloc_cb, _udp_recv_cb);

  _push_io(L, dvr_u, io_u);
  return 1;
}

/*  --- TCP --- */

/* u3_lua_conn: an in-flight TCP connect, carrying the connect callback ref.
*/
typedef struct {
  uv_connect_t req_u;
  u3_lua_io*   io_u;
  int          fn_r;
} u3_lua_conn;

static void
_tcp_read_cb(uv_stream_t* str_u, ssize_t nrd_i, const uv_buf_t* buf_u)
{
  u3_lua_io* io_u = str_u->data;

  if ( nrd_i > 0 ) {
    if ( LUA_NOREF != io_u->cb_r ) {
      lua_State* L = io_u->dvr_u->lua_u->lua;
      lua_rawgeti(L, LUA_REGISTRYINDEX, io_u->cb_r);
      lua_pushlstring(L, buf_u->base, (size_t)nrd_i);
      if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
        u3l_log("lua: %s: tcp recv error: %s",
                io_u->dvr_u->nam_c, lua_tostring(L, -1));
        lua_pop(L, 1);
      }
    }
  }
  else if ( nrd_i < 0 ) {
    _io_kill(io_u);                                  //  EOF or error
  }

  if ( buf_u->base ) {
    c3_free(buf_u->base);
  }
}

static void
_tcp_connect_cb(uv_connect_t* req_u, c3_i sas_i)
{
  u3_lua_conn*   con_u = (u3_lua_conn*)req_u;
  u3_lua_io*     io_u  = con_u->io_u;
  u3_lua_driver* dvr_u = io_u->dvr_u;
  lua_State*     L     = dvr_u->lua_u->lua;
  int            fn_r  = con_u->fn_r;

  if ( sas_i < 0 ) {
    u3l_log("lua: %s: tcp connect failed: %s", dvr_u->nam_c, uv_strerror(sas_i));
    luaL_unref(L, LUA_REGISTRYINDEX, fn_r);
    c3_free(con_u);
    _io_kill(io_u);
    return;
  }
  c3_free(con_u);

  //  hand the connection to the user, then start reading
  //
  lua_rawgeti(L, LUA_REGISTRYINDEX, fn_r);
  _push_io(L, dvr_u, io_u);
  if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
    u3l_log("lua: %s: tcp connect cb error: %s", dvr_u->nam_c, lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, fn_r);

  uv_read_start(&io_u->str_u, _io_alloc_cb, _tcp_read_cb);
}

/* ctx:tcp_connect(host, port, fn): connect; fn(conn) on success.
*/
static int
_ctx_tcp_connect(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:tcp_connect: unavailable without a pier");
  }

  const char* hos_c = luaL_checkstring(L, 2);
  lua_Integer por_i = luaL_checkinteger(L, 3);
  luaL_checktype(L, 4, LUA_TFUNCTION);

  u3_lua_io* io_u = c3_calloc(sizeof(*io_u));
  io_u->typ_y = 'c';
  io_u->dvr_u = dvr_u;
  io_u->cb_r  = LUA_NOREF;
  io_u->uid_d = ++g_uid_d;
  io_u->dead_o = c3n;
  uv_tcp_init(u3L, &io_u->tcp_u);
  io_u->tcp_u.data = io_u;
  io_u->nex_u = dvr_u->io_u;
  dvr_u->io_u = io_u;

  {
    struct sockaddr_in adr_u;
    if ( uv_ip4_addr(hos_c, (c3_i)por_i, &adr_u) ) {
      _io_kill(io_u);
      return luaL_error(L, "ctx:tcp_connect: bad address %s:%d", hos_c, (c3_i)por_i);
    }

    u3_lua_conn* con_u = c3_calloc(sizeof(*con_u));
    con_u->io_u = io_u;
    lua_pushvalue(L, 4);
    con_u->fn_r = luaL_ref(L, LUA_REGISTRYINDEX);

    c3_i r_i = uv_tcp_connect(&con_u->req_u, &io_u->tcp_u,
                              (const struct sockaddr*)&adr_u, _tcp_connect_cb);
    if ( r_i ) {
      luaL_unref(L, LUA_REGISTRYINDEX, con_u->fn_r);
      c3_free(con_u);
      _io_kill(io_u);
      return luaL_error(L, "ctx:tcp_connect: %s", uv_strerror(r_i));
    }
  }
  return 0;
}

static void
_tcp_listen_cb(uv_stream_t* srv_u, c3_i sas_i)
{
  u3_lua_io*     lis_u = srv_u->data;
  u3_lua_driver* dvr_u = lis_u->dvr_u;
  lua_State*     L     = dvr_u->lua_u->lua;

  if ( sas_i < 0 ) {
    u3l_log("lua: %s: tcp listen error: %s", dvr_u->nam_c, uv_strerror(sas_i));
    return;
  }

  u3_lua_io* io_u = c3_calloc(sizeof(*io_u));
  io_u->typ_y = 'c';
  io_u->dvr_u = dvr_u;
  io_u->cb_r  = LUA_NOREF;
  io_u->uid_d = ++g_uid_d;
  io_u->dead_o = c3n;
  uv_tcp_init(u3L, &io_u->tcp_u);
  io_u->tcp_u.data = io_u;

  if ( uv_accept(srv_u, &io_u->str_u) ) {
    uv_close(&io_u->han_u, _io_close_free_cb);
    return;
  }
  io_u->nex_u = dvr_u->io_u;
  dvr_u->io_u = io_u;

  if ( LUA_NOREF != lis_u->cb_r ) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lis_u->cb_r);
    _push_io(L, dvr_u, io_u);
    if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
      u3l_log("lua: %s: tcp accept cb error: %s", dvr_u->nam_c, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  uv_read_start(&io_u->str_u, _io_alloc_cb, _tcp_read_cb);
}

/* ctx:tcp_listen(port, fn): listen on [port]; fn(conn) per accepted connection.
*/
static int
_ctx_tcp_listen(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:tcp_listen: unavailable without a pier");
  }

  lua_Integer por_i = luaL_checkinteger(L, 2);
  luaL_checktype(L, 3, LUA_TFUNCTION);

  u3_lua_io* io_u = c3_calloc(sizeof(*io_u));
  io_u->typ_y = 'l';
  io_u->dvr_u = dvr_u;
  io_u->cb_r  = LUA_NOREF;
  io_u->uid_d = ++g_uid_d;
  io_u->dead_o = c3n;
  uv_tcp_init(u3L, &io_u->tcp_u);
  io_u->tcp_u.data = io_u;

  {
    struct sockaddr_in adr_u;
    uv_ip4_addr("0.0.0.0", (c3_i)por_i, &adr_u);
    c3_i r_i = uv_tcp_bind(&io_u->tcp_u, (const struct sockaddr*)&adr_u, 0);
    if ( !r_i ) {
      r_i = uv_listen(&io_u->str_u, 128, _tcp_listen_cb);
    }
    if ( r_i ) {
      uv_close(&io_u->han_u, _io_close_free_cb);
      return luaL_error(L, "ctx:tcp_listen: :%d: %s", (c3_i)por_i, uv_strerror(r_i));
    }
  }

  lua_pushvalue(L, 3);
  io_u->cb_r  = luaL_ref(L, LUA_REGISTRYINDEX);
  io_u->nex_u = dvr_u->io_u;
  dvr_u->io_u = io_u;

  _push_io(L, dvr_u, io_u);
  return 1;
}

static void
_tcp_write_cb(uv_write_t* req_u, c3_i sas_i)
{
  u3_lua_wreq* wre_u = (u3_lua_wreq*)req_u;
  if ( sas_i < 0 ) {
    u3l_log("lua: %s: tcp write failed: %s", wre_u->nam_c, uv_strerror(sas_i));
  }
  c3_free(wre_u);
}

/*  --- io handle methods (u3.io) --- */

/* io:send(...): UDP -> send(host, port, data); TCP -> send(data).
*/
static int
_io_send(lua_State* L)
{
  u3_lua_io* io_u = _io_resolve(L, 1);
  if ( !io_u ) {
    return luaL_error(L, "io:send: socket closed");
  }

  if ( 'u' == io_u->typ_y ) {
    const char* hos_c = luaL_checkstring(L, 2);
    lua_Integer por_i = luaL_checkinteger(L, 3);
    size_t      len_i;
    const char* dat_c = luaL_checklstring(L, 4, &len_i);

    struct sockaddr_in adr_u;
    if ( uv_ip4_addr(hos_c, (c3_i)por_i, &adr_u) ) {
      return luaL_error(L, "io:send: bad address %s:%d", hos_c, (c3_i)por_i);
    }

    u3_lua_wreq* wre_u = c3_malloc(sizeof(*wre_u) + len_i);
    memcpy(wre_u->dat_y, dat_c, len_i);
    wre_u->nam_c        = io_u->dvr_u->nam_c;
    wre_u->udp_u.data   = wre_u;
    uv_buf_t buf_u = uv_buf_init((c3_c*)wre_u->dat_y, len_i);
    c3_i r_i = uv_udp_send(&wre_u->udp_u, &io_u->udp_u, &buf_u, 1,
                           (const struct sockaddr*)&adr_u, _udp_send_cb);
    if ( r_i ) {
      c3_free(wre_u);
      return luaL_error(L, "io:send: %s", uv_strerror(r_i));
    }
  }
  else {
    //  tcp connection
    //
    size_t      len_i;
    const char* dat_c = luaL_checklstring(L, 2, &len_i);

    u3_lua_wreq* wre_u = c3_malloc(sizeof(*wre_u) + len_i);
    memcpy(wre_u->dat_y, dat_c, len_i);
    wre_u->nam_c      = io_u->dvr_u->nam_c;
    wre_u->tcp_u.data = wre_u;
    uv_buf_t buf_u = uv_buf_init((c3_c*)wre_u->dat_y, len_i);
    c3_i r_i = uv_write(&wre_u->tcp_u, &io_u->str_u, &buf_u, 1, _tcp_write_cb);
    if ( r_i ) {
      c3_free(wre_u);
      return luaL_error(L, "io:send: %s", uv_strerror(r_i));
    }
  }
  return 0;
}

/* io:recv(fn): set/replace the receive handler.
*/
static int
_io_recv(lua_State* L)
{
  u3_lua_io* io_u = _io_resolve(L, 1);
  if ( !io_u ) {
    return luaL_error(L, "io:recv: socket closed");
  }
  luaL_checktype(L, 2, LUA_TFUNCTION);

  if ( LUA_NOREF != io_u->cb_r ) {
    luaL_unref(L, LUA_REGISTRYINDEX, io_u->cb_r);
  }
  lua_pushvalue(L, 2);
  io_u->cb_r = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

/* io:close(): close the socket (idempotent).
*/
static int
_io_close(lua_State* L)
{
  u3_lua_io* io_u = _io_resolve(L, 1);
  if ( io_u ) {
    _io_kill(io_u);
  }
  return 0;
}

static const luaL_Reg _io_methods[] = {
  { "send",  _io_send  },
  { "recv",  _io_recv  },
  { "close", _io_close },
  { 0, 0 }
};

/* _lua_reg_io(): install the "u3.io" metatable (methods via __index).
*/
static void
_lua_reg_io(lua_State* L)
{
  luaL_newmetatable(L, U3_LUA_IO);
  lua_newtable(L);
  luaL_setfuncs(L, _io_methods, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

/*  ------------------------------------------------------------------------
 *  pier filesystem: pier-scoped file access for drivers
 *
 *  All paths are relative to the pier root, with absolute paths and ".."/"."
 *  components rejected so a driver cannot escape the pier. A driver is trusted
 *  code the user dropped in, so it gets the whole pier -- but writing under
 *  .urb/ (the event log, checkpoints) can corrupt the ship; do so knowingly.
 *  ------------------------------------------------------------------------ */

/* _fs_path_safe(): reject absolute paths and any "."/".." path component.
*/
static c3_o
_fs_path_safe(const c3_c* rel_c)
{
  if ( !rel_c || (rel_c[0] == '/') ) {
    return c3n;
  }
  const c3_c* p_c = rel_c;
  while ( *p_c ) {
    const c3_c* seg_c = p_c;
    while ( *p_c && (*p_c != '/') ) {
      p_c++;
    }
    c3_w len_w = (c3_w)(p_c - seg_c);
    if (  (1 == len_w) && ('.' == seg_c[0]) ) return c3n;
    if (  (2 == len_w) && ('.' == seg_c[0]) && ('.' == seg_c[1]) ) return c3n;
    if ( *p_c == '/' ) p_c++;
  }
  return c3y;
}

/* _fs_resolve(): malloc'd "<pier>/<rel>" if safe, else 0. "" or "." -> root.
*/
static c3_c*
_fs_resolve(u3_lua_driver* dvr_u, const c3_c* rel_c)
{
  if ( !dvr_u->car_u.pir_u ) {
    return 0;
  }
  u3_pier* pir_u = (u3_pier*)dvr_u->car_u.pir_u;

  if ( (0 == rel_c[0]) || ((rel_c[0] == '.') && (0 == rel_c[1])) ) {
    return _lua_strdup(pir_u->pax_c);            //  pier root itself
  }
  if ( c3n == _fs_path_safe(rel_c) ) {
    return 0;
  }
  return _lua_join(pir_u->pax_c, rel_c);
}

/* ctx:pier_path(): the absolute path of the pier directory.
*/
static int
_ctx_pier_path(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:pier_path: unavailable without a pier");
  }
  lua_pushstring(L, ((u3_pier*)dvr_u->car_u.pir_u)->pax_c);
  return 1;
}

/* ctx:read(path): file contents as a string, or nil if it can't be read.
*/
static int
_ctx_read(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:read: unsafe path or no pier");
  }

  size_t len_i = 0;
  c3_y*  dat_y = _lua_slurp(pax_c, &len_i);
  c3_free(pax_c);

  if ( !dat_y ) {
    lua_pushnil(L);
  }
  else {
    lua_pushlstring(L, (const char*)dat_y, len_i);
    c3_free(dat_y);
  }
  return 1;
}

/* ctx:write(path, data): write [data] to a file under the pier (overwrites).
*/
static int
_ctx_write(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  size_t         len_i;
  const c3_c*    dat_c = luaL_checklstring(L, 3, &len_i);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:write: unsafe path or no pier");
  }

  FILE* fil_u = fopen(pax_c, "wb");
  c3_free(pax_c);
  if ( !fil_u ) {
    return luaL_error(L, "ctx:write: cannot open %s", rel_c);
  }
  size_t wit_i = fwrite(dat_c, 1, len_i, fil_u);
  fclose(fil_u);
  if ( wit_i != len_i ) {
    return luaL_error(L, "ctx:write: short write to %s", rel_c);
  }
  return 0;
}

/* ctx:list([path]): array of entry names in a directory (default: pier root).
*/
static int
_ctx_list(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_optstring(L, 2, ".");
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:list: unsafe path or no pier");
  }

  DIR* dir_u = opendir(pax_c);
  c3_free(pax_c);
  if ( !dir_u ) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  {
    struct dirent* den_u;
    int            idx_i = 1;
    while ( (den_u = readdir(dir_u)) ) {
      if (  (0 == strcmp(den_u->d_name, "."))
         || (0 == strcmp(den_u->d_name, "..")) ) {
        continue;
      }
      lua_pushstring(L, den_u->d_name);
      lua_rawseti(L, -2, idx_i++);
    }
  }
  closedir(dir_u);
  return 1;
}

/* ctx:stat(path): {kind=, size=, mtime=} for a path, or nil if absent.
*/
static int
_ctx_stat(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:stat: unsafe path or no pier");
  }

  struct stat buf_u;
  c3_i        ret_i = stat(pax_c, &buf_u);
  c3_free(pax_c);

  if ( 0 != ret_i ) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  lua_pushstring(L, S_ISDIR(buf_u.st_mode) ? "dir" : "file");
  lua_setfield(L, -2, "kind");
  lua_pushinteger(L, (lua_Integer)buf_u.st_size);
  lua_setfield(L, -2, "size");
  lua_pushinteger(L, (lua_Integer)buf_u.st_mtime);
  lua_setfield(L, -2, "mtime");
  return 1;
}

/* ctx:exists(path): boolean.
*/
static int
_ctx_exists(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:exists: unsafe path or no pier");
  }
  struct stat buf_u;
  lua_pushboolean(L, (0 == stat(pax_c, &buf_u)));
  c3_free(pax_c);
  return 1;
}

/* ctx:mkdir(path): create a directory under the pier (no-op if it exists).
*/
static int
_ctx_mkdir(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:mkdir: unsafe path or no pier");
  }
  c3_i ret_i = mkdir(pax_c, 0700);
  c3_free(pax_c);
  if ( (0 != ret_i) && (EEXIST != errno) ) {
    return luaL_error(L, "ctx:mkdir: %s: %s", rel_c, strerror(errno));
  }
  return 0;
}

/* ctx:remove(path): delete a file or (empty) directory under the pier.
*/
static int
_ctx_remove(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  const c3_c*    rel_c = luaL_checkstring(L, 2);
  c3_c*          pax_c = _fs_resolve(dvr_u, rel_c);
  if ( !pax_c ) {
    return luaL_error(L, "ctx:remove: unsafe path or no pier");
  }

  struct stat buf_u;
  c3_i        ret_i;
  if ( (0 == stat(pax_c, &buf_u)) && S_ISDIR(buf_u.st_mode) ) {
    ret_i = rmdir(pax_c);
  }
  else {
    ret_i = unlink(pax_c);
  }
  c3_free(pax_c);
  if ( 0 != ret_i ) {
    return luaL_error(L, "ctx:remove: %s: %s", rel_c, strerror(errno));
  }
  return 0;
}

static const luaL_Reg _ctx_methods[] = {
  { "log",         _ctx_log         },
  { "plan",        _ctx_plan        },
  { "wish",        _ctx_wish        },
  { "after",       _ctx_after       },
  { "every",       _ctx_every       },
  { "udp_open",    _ctx_udp_open    },
  { "tcp_connect", _ctx_tcp_connect },
  { "tcp_listen",  _ctx_tcp_listen  },
  { "pier_path",   _ctx_pier_path   },
  { "read",        _ctx_read        },
  { "write",       _ctx_write       },
  { "list",        _ctx_list        },
  { "stat",        _ctx_stat        },
  { "exists",      _ctx_exists      },
  { "mkdir",       _ctx_mkdir       },
  { "remove",      _ctx_remove      },
  { 0, 0 }
};

/* _lua_reg_ctx(): install the "u3.ctx" metatable (methods via __index).
*/
static void
_lua_reg_ctx(lua_State* L)
{
  luaL_newmetatable(L, U3_LUA_CTX);
  lua_newtable(L);
  luaL_setfuncs(L, _ctx_methods, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

/* _lua_reg_noun(): install the "u3.noun" metatable and the `noun` module.
*/
static void
_lua_reg_noun(lua_State* L)
{
  luaL_newmetatable(L, U3_LUA_NOUN);
  lua_pushcfunction(L, _noun_gc);       lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, _noun_tostring); lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, _noun_eq_mt);    lua_setfield(L, -2, "__eq");
  lua_pop(L, 1);

  luaL_newlib(L, _noun_funcs);
  lua_setglobal(L, "noun");
}

/* _lua_open_libs(): open a sandboxed subset of the standard library.
**
**   Deliberately omits io/os/package, and removes the filesystem code-loading
**   escapes (dofile/loadfile) from the base library. `load` is retained: the
**   loader uses it, and without dlopen it cannot reach native code.
*/
static void
_lua_open_libs(lua_State* L)
{
  static const luaL_Reg lib_u[] = {
    { LUA_GNAME,       luaopen_base      },
    { LUA_TABLIBNAME,  luaopen_table     },
    { LUA_STRLIBNAME,  luaopen_string    },
    { LUA_MATHLIBNAME, luaopen_math      },
    { LUA_COLIBNAME,   luaopen_coroutine },
    { LUA_UTF8LIBNAME, luaopen_utf8      },
    { 0, 0 }
  };

  for ( const luaL_Reg* lib = lib_u; lib->func; lib++ ) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);
  }

  lua_pushnil(L); lua_setglobal(L, "dofile");
  lua_pushnil(L); lua_setglobal(L, "loadfile");
}

u3_lua*
u3_lua_boot(struct _u3_pier* pir_u)
{
  lua_State* L = luaL_newstate();
  if ( !L ) {
    return 0;
  }

  _lua_open_libs(L);
  _lua_reg_noun(L);
  _lua_reg_ctx(L);
  _lua_reg_io(L);

  {
    u3_lua* lua_u = c3_calloc(sizeof(*lua_u));
    lua_u->lua   = L;
    lua_u->pir_u = pir_u;
    return lua_u;
  }
}

void
u3_lua_halt(u3_lua* lua_u)
{
  if ( !lua_u ) {
    return;
  }
  if ( lua_u->lua ) {
    lua_close(lua_u->lua);               //  runs __gc on all live nouns
  }
  c3_free(lua_u);
}

void
u3_lua_push_noun(lua_State* L, u3_noun som)
{
  _push_owned(L, u3k(som));
}

u3_noun
u3_lua_get_noun(lua_State* L, c3_i idx)
{
  return _check_noun(L, (int)idx);
}

/*  ------------------------------------------------------------------------
 *  meta-driver: a u3_auto whose talk/kick/exit trampoline into a Lua table
 *  ------------------------------------------------------------------------ */

/* _lua_strdup(): malloc a copy of [str_c] via c3_malloc.
*/
static c3_c*
_lua_strdup(const c3_c* str_c)
{
  c3_w  len_w = (c3_w)strlen(str_c);
  c3_c* out_c = c3_malloc(len_w + 1);
  memcpy(out_c, str_c, len_w + 1);
  return out_c;
}

/* _lua_join(): "<dir_c>/<nam_c>", malloced.
*/
static c3_c*
_lua_join(const c3_c* dir_c, const c3_c* nam_c)
{
  c3_w  dir_w = (c3_w)strlen(dir_c);
  c3_w  nam_w = (c3_w)strlen(nam_c);
  c3_c* out_c = c3_malloc(dir_w + 1 + nam_w + 1);

  memcpy(out_c, dir_c, dir_w);
  out_c[dir_w] = '/';
  memcpy(out_c + dir_w + 1, nam_c, nam_w + 1);
  return out_c;
}

/* _lua_is_script(): does [nam_c] end in ".lua" (with a non-empty stem)?
*/
static c3_o
_lua_is_script(const c3_c* nam_c)
{
  c3_w len_w = (c3_w)strlen(nam_c);
  return ( (len_w > 4) && (0 == strcmp(nam_c + len_w - 4, ".lua")) )
         ? c3y : c3n;
}

/* _lua_slurp(): read the whole file at [pax_c] into a malloced buffer.
*/
static c3_y*
_lua_slurp(const c3_c* pax_c, size_t* len_i)
{
  FILE* fil_u = fopen(pax_c, "rb");
  if ( !fil_u ) {
    return 0;
  }

  if ( 0 != fseek(fil_u, 0, SEEK_END) ) { fclose(fil_u); return 0; }
  long siz_i = ftell(fil_u);
  if ( siz_i < 0 ) { fclose(fil_u); return 0; }
  rewind(fil_u);

  c3_y*  buf_y = c3_malloc((size_t)siz_i + 1);
  size_t red_i = fread(buf_y, 1, (size_t)siz_i, fil_u);
  fclose(fil_u);

  if ( red_i != (size_t)siz_i ) {
    c3_free(buf_y);
    return 0;
  }
  buf_y[siz_i] = 0;
  *len_i = (size_t)siz_i;
  return buf_y;
}

/* _lua_call_self(): invoke driver table method [nam_c] as fn(ctx), if present.
*/
static void
_lua_call_self(u3_lua_driver* dvr_u, const c3_c* nam_c)
{
  lua_State* L = dvr_u->lua_u->lua;

  lua_rawgeti(L, LUA_REGISTRYINDEX, dvr_u->tab_r);   //  driver table
  lua_getfield(L, -1, nam_c);                        //  table[nam_c]

  if ( lua_isfunction(L, -1) ) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, dvr_u->ctx_r); //  ctx
    if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
      u3l_log("lua: %s: %s error: %s",
              dvr_u->nam_c, nam_c, lua_tostring(L, -1));
      lua_pop(L, 1);                                 //  error message
    }
  }
  else {
    lua_pop(L, 1);                                   //  non-function field
  }
  lua_pop(L, 1);                                     //  driver table
}

/* _lua_io_talk(): startup -- run the driver's `talk`, mark live.
*/
static void
_lua_io_talk(u3_auto* car_u)
{
  u3_lua_driver* dvr_u = (u3_lua_driver*)car_u;
  _lua_call_self(dvr_u, "talk");
  car_u->liv_o = c3y;
}

/* _lua_io_kick(): route an effect to the driver's `kick(ctx, wire, card)`.
**
**   Per the u3_auto contract this owns [wir]/[cad] and must dispose them.
**   The nouns handed to Lua are RETAINed (Lua gets its own GC-managed refs).
*/
static c3_o
_lua_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_lua_driver* dvr_u = (u3_lua_driver*)car_u;
  lua_State*     L     = dvr_u->lua_u->lua;
  c3_o           ret_o = c3n;

  lua_rawgeti(L, LUA_REGISTRYINDEX, dvr_u->tab_r);   //  driver table
  lua_getfield(L, -1, "kick");                       //  table.kick

  if ( lua_isfunction(L, -1) ) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, dvr_u->ctx_r); //  ctx
    u3_lua_push_noun(L, wir);                        //  RETAIN
    u3_lua_push_noun(L, cad);                        //  RETAIN

    if ( LUA_OK == lua_pcall(L, 3, 1, 0) ) {
      ret_o = lua_toboolean(L, -1) ? c3y : c3n;
    }
    else {
      u3l_log("lua: %s: kick error: %s",
              dvr_u->nam_c, lua_tostring(L, -1));
    }
    lua_pop(L, 1);                                   //  result or error
  }
  else {
    lua_pop(L, 1);                                   //  non-function kick
  }
  lua_pop(L, 1);                                     //  driver table

  u3z(wir); u3z(cad);                                //  dispose owned refs
  return ret_o;
}

/* _lua_io_exit(): teardown -- run `exit`, release refs, free; close state last.
*/
static void
_lua_io_exit(u3_auto* car_u)
{
  u3_lua_driver* dvr_u = (u3_lua_driver*)car_u;
  u3_lua*        lua_u = dvr_u->lua_u;
  lua_State*     L     = lua_u->lua;

  _lua_call_self(dvr_u, "exit");

  _lua_close_timers(dvr_u);                          //  stop/close libuv timers
  _lua_close_io(dvr_u);                              //  stop/close libuv sockets
  luaL_unref(L, LUA_REGISTRYINDEX, dvr_u->ctx_r);
  luaL_unref(L, LUA_REGISTRYINDEX, dvr_u->tab_r);
  c3_free(dvr_u->nam_c);
  c3_free(dvr_u);

  //  each driver owns its own interpreter; close it
  //
  u3_lua_halt(lua_u);
}

/* _make_ctx(): create a ctx userdata for [dvr_u], returning a registry ref.
*/
static int
_make_ctx(lua_State* L, u3_lua_driver* dvr_u)
{
  u3_lua_driver** box = (u3_lua_driver**)lua_newuserdatauv(L, sizeof(dvr_u), 0);
  *box = dvr_u;
  luaL_setmetatable(L, U3_LUA_CTX);
  return luaL_ref(L, LUA_REGISTRYINDEX);             //  pops, returns ref
}

/* _lua_load_one(): load a single script into a driver, or 0 on failure.
*/
static u3_lua_driver*
_lua_load_one(struct _u3_pier* pir_u, const c3_c* dir_c, const c3_c* nam_c)
{
  c3_c*  pax_c = _lua_join(dir_c, nam_c);
  size_t len_i = 0;
  c3_y*  src_y = _lua_slurp(pax_c, &len_i);

  if ( !src_y ) {
    u3l_log("lua: cannot read %s", pax_c);
    c3_free(pax_c);
    return 0;
  }

  //  each driver runs in its own isolated lua_State, so a misbehaving driver
  //  cannot reach into another's globals, and reload is a clean lua_close.
  //
  u3_lua* lua_u = u3_lua_boot(pir_u);
  if ( !lua_u ) {
    u3l_log("lua: %s: out of memory booting interpreter", nam_c);
    c3_free(src_y); c3_free(pax_c);
    return 0;
  }
  lua_State* L = lua_u->lua;

  if ( LUA_OK != luaL_loadbuffer(L, (const char*)src_y, len_i, nam_c) ) {
    u3l_log("lua: %s: load error: %s", nam_c, lua_tostring(L, -1));
    c3_free(src_y); c3_free(pax_c);
    u3_lua_halt(lua_u);
    return 0;
  }
  c3_free(src_y);
  c3_free(pax_c);

  if ( LUA_OK != lua_pcall(L, 0, 1, 0) ) {
    u3l_log("lua: %s: run error: %s", nam_c, lua_tostring(L, -1));
    u3_lua_halt(lua_u);
    return 0;
  }

  if ( !lua_istable(L, -1) ) {
    u3l_log("lua: %s: script must `return` a driver table", nam_c);
    u3_lua_halt(lua_u);
    return 0;
  }

  //  read name (default: filename) and priority (default 100)
  //
  c3_c* dnam_c = 0;
  lua_getfield(L, -1, "name");
  if ( lua_isstring(L, -1) ) {
    dnam_c = _lua_strdup(lua_tostring(L, -1));
  }
  lua_pop(L, 1);
  if ( !dnam_c ) {
    dnam_c = _lua_strdup(nam_c);
  }

  c3_w pri_w = 100;
  lua_getfield(L, -1, "priority");
  if ( lua_isinteger(L, -1) ) {
    lua_Integer p_i = lua_tointeger(L, -1);
    pri_w = (p_i < 0) ? 0 : (c3_w)p_i;
  }
  lua_pop(L, 1);

  //  consume the table into a registry ref
  //
  int tab_r = luaL_ref(L, LUA_REGISTRYINDEX);

  //  build the wrapper
  //
  u3_lua_driver* dvr_u = c3_calloc(sizeof(*dvr_u));
  dvr_u->lua_u = lua_u;
  dvr_u->pri_w = pri_w;
  dvr_u->nam_c = dnam_c;
  dvr_u->tab_r = tab_r;
  dvr_u->ctx_r = _make_ctx(L, dvr_u);

  {
    u3_auto* car_u   = &dvr_u->car_u;
    car_u->nam_m     = c3__lua;
    car_u->liv_o     = c3n;
    car_u->pir_u     = (u3_pier*)lua_u->pir_u;        //  0 in tests
    car_u->io.talk_f = _lua_io_talk;
    car_u->io.kick_f = _lua_io_kick;
    car_u->io.exit_f = _lua_io_exit;
  }

  return dvr_u;
}

struct _u3_auto*
u3_lua_load_dir(struct _u3_pier* pir_u, const c3_c* dir_c)
{
  u3_dire* fol_u = u3_foil_folder(dir_c);
  if ( !fol_u ) {
    return 0;
  }

  //  count candidate scripts, then load (each in its own isolated state)
  //
  c3_w max_w = 0;
  for ( u3_dent* den_u = fol_u->all_u; den_u; den_u = den_u->nex_u ) {
    if ( c3y == _lua_is_script(den_u->nam_c) ) {
      max_w++;
    }
  }

  if ( 0 == max_w ) {
    u3_dire_free(fol_u);
    return 0;
  }

  u3_lua_driver** all_u = c3_calloc(max_w * sizeof(*all_u));
  c3_w            cnt_w = 0;

  for ( u3_dent* den_u = fol_u->all_u; den_u; den_u = den_u->nex_u ) {
    if ( c3y == _lua_is_script(den_u->nam_c) ) {
      u3_lua_driver* dvr_u = _lua_load_one(pir_u, dir_c, den_u->nam_c);
      if ( dvr_u ) {
        all_u[cnt_w++] = dvr_u;
      }
    }
  }
  u3_dire_free(fol_u);

  if ( 0 == cnt_w ) {
    c3_free(all_u);
    return 0;
  }

  //  stable insertion sort by ascending priority (ties keep scan order)
  //
  for ( c3_w i_w = 1; i_w < cnt_w; i_w++ ) {
    u3_lua_driver* key_u = all_u[i_w];
    c3_ws          j_ws  = (c3_ws)i_w - 1;
    while ( (j_ws >= 0) && (all_u[j_ws]->pri_w > key_u->pri_w) ) {
      all_u[j_ws + 1] = all_u[j_ws];
      j_ws--;
    }
    all_u[j_ws + 1] = key_u;
  }

  //  link into a sub-chain: head = lowest priority, tail->nex_u = 0
  //
  for ( c3_w i_w = 0; i_w < cnt_w; i_w++ ) {
    all_u[i_w]->car_u.nex_u =
      (i_w + 1 < cnt_w) ? &all_u[i_w + 1]->car_u : 0;
  }
  u3_auto* hed_u = &all_u[0]->car_u;
  c3_free(all_u);

  u3l_log("lua: loaded %u driver%s from %s",
          cnt_w, (1 == cnt_w) ? "" : "s", dir_c);
  return hed_u;
}

struct _u3_auto*
u3_lua_init(struct _u3_pier* pir_u)
{
  u3_pier* p_u   = (u3_pier*)pir_u;
  c3_c*    dir_c = _lua_join(p_u->pax_c, "lua");
  u3_auto* hed_u = u3_lua_load_dir(pir_u, dir_c);
  c3_free(dir_c);
  return hed_u;
}

/*  ------------------------------------------------------------------------
 *  hot reload: watch $pier/lua/ and rebuild the driver block on change
 *  ------------------------------------------------------------------------ */

/* u3_lua_load: process-wide manager for the watched Lua block.
*/
typedef struct _u3_lua_load {
  struct _u3_pier* pir_u;             //  pier
  c3_c*            dir_c;             //  $pier/lua (kept for the process)
  u3_auto*         anc_u;             //  anchor; block lives at anc_u->nex_u
  uv_fs_event_t    fsv_u;             //  directory watcher
  uv_timer_t       deb_u;            //  debounce timer
} u3_lua_load;

static u3_lua_load* s_lua_load = 0;    //  one pier per king process

/* _lua_splice(): link the [hed_u] block (priority-sorted, null-terminated)
**   into the chain immediately after [anc_u].
*/
static void
_lua_splice(u3_auto* anc_u, u3_auto* hed_u)
{
  if ( hed_u ) {
    u3_auto* tal_u = hed_u;
    while ( tal_u->nex_u ) {
      tal_u = tal_u->nex_u;
    }
    tal_u->nex_u = anc_u->nex_u;
    anc_u->nex_u = hed_u;
  }
}

/* _lua_reload(): tear down the current Lua block and rebuild it from disk.
**
**   Runs from the (debounced) watcher callback, i.e. on the libuv loop and
**   never during effect dispatch, so chain surgery is reentrancy-safe.
**
**   NB: drivers with Arvo events *in flight* at the instant of reload are a
**   known edge -- their freed driver would be referenced on completion. Typical
**   drivers (timers/sockets that inject only intermittently) reload cleanly;
**   a driver mid-injection should be quiesced before its file is changed.
*/
static void
_lua_reload(u3_lua_load* lod_u)
{
  u3_auto* anc_u = lod_u->anc_u;

  //  detach the contiguous run of Lua drivers after the anchor
  //
  u3_auto* old_u = anc_u->nex_u;
  u3_auto* tal_u = 0;
  u3_auto* cur_u = old_u;
  while ( cur_u && (c3__lua == cur_u->nam_m) ) {
    tal_u = cur_u;
    cur_u = cur_u->nex_u;
  }
  u3_auto* rst_u = cur_u;              //  first built-in after the block (or 0)

  if ( tal_u ) {
    anc_u->nex_u = rst_u;             //  unlink the block
    tal_u->nex_u = 0;                //  null-terminate it
    u3_auto_exit(old_u);             //  exit + free each old driver
  }

  //  rebuild from disk, start the new drivers, splice back in
  //
  u3_auto* new_u = u3_lua_load_dir(lod_u->pir_u, lod_u->dir_c);
  if ( new_u ) {
    u3_auto_talk(new_u);             //  block is null-terminated -> talks only it
    _lua_splice(anc_u, new_u);
  }

  u3l_log("lua: reloaded drivers from %s", lod_u->dir_c);
}

/* _lua_deb_cb(): debounce expiry -> perform the reload.
*/
static void
_lua_deb_cb(uv_timer_t* tim_u)
{
  _lua_reload((u3_lua_load*)tim_u->data);
}

/* _lua_fs_cb(): a change in $pier/lua/ -> (re)arm the debounce timer.
**
**   Editors emit several events per save; debouncing coalesces them into one
**   reload ~250ms after the last change.
*/
static void
_lua_fs_cb(uv_fs_event_t* fsv_u, const char* nam_c, c3_i evt_i, c3_i sas_i)
{
  u3_lua_load* lod_u = fsv_u->data;
  uv_timer_start(&lod_u->deb_u, _lua_deb_cb, 250, 0);   //  restart = debounce
}

void
u3_lua_attach(struct _u3_pier* pir_u, struct _u3_auto* anc_u)
{
  u3_pier* p_u   = (u3_pier*)pir_u;
  c3_c*    dir_c = _lua_join(p_u->pax_c, "lua");

  //  initial load + splice (the pier talks these itself, after u3_auto_init)
  //
  _lua_splice(anc_u, u3_lua_load_dir(pir_u, dir_c));

  //  manager + directory watcher for live reloads
  //
  u3_lua_load* lod_u = c3_calloc(sizeof(*lod_u));
  lod_u->pir_u = pir_u;
  lod_u->dir_c = dir_c;               //  kept for the process lifetime
  lod_u->anc_u = anc_u;
  s_lua_load   = lod_u;

  uv_timer_init(u3L, &lod_u->deb_u);
  lod_u->deb_u.data = lod_u;

  if ( 0 == uv_fs_event_init(u3L, &lod_u->fsv_u) ) {
    lod_u->fsv_u.data = lod_u;
    c3_i r_i = uv_fs_event_start(&lod_u->fsv_u, _lua_fs_cb, dir_c, 0);
    if ( r_i ) {
      u3l_log("lua: cannot watch %s: %s", dir_c, uv_strerror(r_i));
    }
    else {
      u3l_log("lua: watching %s for live reload", dir_c);
    }
  }
}
