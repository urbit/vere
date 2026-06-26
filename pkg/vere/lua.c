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
#include <sys/stat.h>
#include <curl/curl.h>
#include <luv/luv.h>

#define U3_LUA_NOUN  "u3.noun"          //  metatable name for noun userdata
#define U3_LUA_CTX   "u3.ctx"           //  metatable name for driver context

/* u3_lua_driver: a u3_auto driver backed by a Lua table.
**
**   The embedded u3_auto must be the first field so a u3_auto* can be cast to
**   u3_lua_driver*. [tab_r]/[ctx_r] are luaL_ref handles into the registry for
**   the driver table and its ctx userdata; [lua_u] is the shared, ref-counted
**   bridge state.
*/
  typedef struct _u3_lua_driver {
    u3_auto  car_u;                     //  driver (first field)
    u3_lua*  lua_u;                     //  this driver's isolated state
    c3_w     pri_w;                     //  priority (lower = earlier)
    c3_c*    nam_c;                     //  driver name (for logging)
    c3_c*    src_c;                     //  source filename (for incremental reload)
    c3_d     mod_d;                     //  source mtime (ns), for change detection
    c3_d     uid_d;                     //  liveness id (live-driver registry)
    int      tab_r;                     //  registry ref: driver table
    int      ctx_r;                     //  registry ref: ctx userdata
  } u3_lua_driver;

//  small helpers defined later in this file
//
static c3_c* _lua_strdup(const c3_c* str_c);
static c3_c* _lua_join(const c3_c* dir_c, const c3_c* nam_c);
static c3_y* _lua_slurp(const c3_c* pax_c, size_t* len_i);

static c3_d g_uid_d = 0;               //  monotonic id for handles/drivers

/*  ------------------------------------------------------------------------
 *  live-driver registry
 *
 *  Async operations (DNS resolve, TCP connect, scry, HTTP) can complete after
 *  their driver is torn down by exit or hot reload. Each driver has a unique
 *  [uid_d]; async callbacks store only that id and look the driver up here.
 *  If the lookup misses, the driver is gone (and its lua_State closed), so the
 *  callback drops the result without ever dereferencing freed memory.
 *  ------------------------------------------------------------------------ */

typedef struct _u3_live {
  c3_d             uid_d;
  u3_lua_driver*   dvr_u;
  struct _u3_live* nex_u;
} u3_live;

static u3_live* s_live = 0;

static void
_live_add(u3_lua_driver* dvr_u)
{
  u3_live* liv_u = c3_calloc(sizeof(*liv_u));
  liv_u->uid_d = dvr_u->uid_d;
  liv_u->dvr_u = dvr_u;
  liv_u->nex_u = s_live;
  s_live = liv_u;
}

static void
_live_del(c3_d uid_d)
{
  u3_live** pre_u = &s_live;
  while ( *pre_u ) {
    if ( (*pre_u)->uid_d == uid_d ) {
      u3_live* die_u = *pre_u;
      *pre_u = die_u->nex_u;
      c3_free(die_u);
      return;
    }
    pre_u = &(*pre_u)->nex_u;
  }
}

/* _live_get(): the live driver for [uid_d], or 0 if it's gone.
*/
static u3_lua_driver*
_live_get(c3_d uid_d)
{
  for ( u3_live* liv_u = s_live; liv_u; liv_u = liv_u->nex_u ) {
    if ( liv_u->uid_d == uid_d ) {
      return liv_u->dvr_u;
    }
  }
  return 0;
}

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
/*  ------------------------------------------------------------------------
 *  scry: read Arvo's namespace (the read-side complement to ctx:plan)
 *  ------------------------------------------------------------------------ */

/* u3_lua_scry: an in-flight namespace read (uid-keyed for liveness).
*/
typedef struct {
  c3_d dvr_uid;
  int  fn_r;
} u3_lua_scry;

/* _path_to_knots(): "/a/b" or "a/b" -> the knot list [%a %b ~].
*/
static u3_noun
_path_to_knots(const c3_c* p_c)
{
  while ( '/' == *p_c ) {
    p_c++;
  }
  if ( 0 == *p_c ) {
    return u3_nul;
  }
  const c3_c* seg_c = p_c;
  while ( *p_c && ('/' != *p_c) ) {
    p_c++;
  }
  return u3nc(u3i_bytes((c3_w)(p_c - seg_c), (const c3_y*)seg_c),
             _path_to_knots(p_c));
}

/* _lua_scry_cb(): namespace read response -> fn(result|nil). OWNS [rep].
*/
static void
_lua_scry_cb(void* ptr_v, u3_noun rep)
{
  u3_lua_scry*   scr_u = ptr_v;
  u3_lua_driver* dvr_u = _live_get(scr_u->dvr_uid);

  if ( dvr_u ) {
    lua_State* L = dvr_u->lua_u->lua;
    lua_rawgeti(L, LUA_REGISTRYINDEX, scr_u->fn_r);
    if ( (u3_nul == rep) || (c3n == u3du(rep)) ) {
      lua_pushnil(L);                                //  no result
    }
    else {
      u3_lua_push_noun(L, u3t(rep));                 //  unwrap [~ dat], gains
    }
    if ( LUA_OK != lua_pcall(L, 1, 0, 0) ) {
      u3l_log("lua: %s: scry error: %s", dvr_u->nam_c, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, scr_u->fn_r);
  }

  u3z(rep);
  c3_free(scr_u);
}

/* ctx:scry(care, desk, path, fn): read the namespace; fn(result|nil).
**
**   [care] is the vane+care mote string (e.g. "cx" clay file, "cy" arch,
**   "ax" ames, "gx" gall). [desk] is a desk string for clay, or nil. [path]
**   is a "/a/b/c" path. Ship and case (now) are injected automatically.
*/
static int
_ctx_scry(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:scry: unavailable without a pier");
  }

  size_t      cln_i;
  const c3_c* car_c = luaL_checklstring(L, 2, &cln_i);
  if ( (0 == cln_i) || (cln_i > 4) ) {
    return luaL_error(L, "ctx:scry: care must be 1-4 chars");
  }
  const c3_c* dsk_c = luaL_optstring(L, 3, 0);       //  nil -> no desk
  const c3_c* pax_c = luaL_checkstring(L, 4);
  luaL_checktype(L, 5, LUA_TFUNCTION);

  //  pack the care string into a mote (LSB-first cord)
  //
  c3_m car_m = 0;
  for ( size_t i_i = 0; i_i < cln_i; i_i++ ) {
    car_m |= ((c3_m)(c3_y)car_c[i_i]) << (8 * i_i);
  }

  u3_atom des = ( dsk_c && dsk_c[0] ) ? u3i_string(dsk_c) : u3_nul;
  u3_noun pax = _path_to_knots(pax_c);

  u3_lua_scry* scr_u = c3_calloc(sizeof(*scr_u));
  scr_u->dvr_uid = dvr_u->uid_d;
  lua_pushvalue(L, 5);
  scr_u->fn_r = luaL_ref(L, LUA_REGISTRYINDEX);

  //  gang [~ ~]: full local access, as the control plane (conn) uses
  //
  u3_pier_peek_last((u3_pier*)dvr_u->car_u.pir_u,
                    u3nc(u3_nul, u3_nul),
                    car_m, des, pax,
                    scr_u, _lua_scry_cb);
  return 0;
}

/*  ------------------------------------------------------------------------
 *  HTTP client: libcurl's multi interface driven by the libuv loop
 *
 *  One shared curl multi handle + one uv_timer (curl's notion of timeouts),
 *  lazily initialized. curl tells us which sockets to watch via the socket
 *  callback; we drive them with uv_poll_t. Completions are detected with
 *  curl_multi_info_read and dispatched to the requesting driver (uid-keyed).
 *  ------------------------------------------------------------------------ */

typedef struct {
  CURLM*     mul_u;                    //  curl multi handle
  uv_timer_t tim_u;                    //  curl timeout timer
  c3_o       ini_o;                    //  initialized?
} u3_curl_glob;

static u3_curl_glob g_curl = { 0, {0}, c3n };

typedef struct {                       //  per-watched-socket
  uv_poll_t     pol_u;
  curl_socket_t sok_i;
} u3_curl_ctx;

typedef struct {                       //  per-request
  CURL*              ezy_u;            //  easy handle
  c3_d               dvr_uid;          //  liveness
  int                fn_r;             //  completion callback
  c3_c*              bod_c;            //  response body
  size_t             bln_i;
  c3_c*              hdr_c;            //  response headers (raw)
  size_t             hln_i;
  struct curl_slist* shd_u;            //  request headers to free
} u3_curl_req;

static void
_curl_check_info(void)
{
  CURLMsg* msg_u;
  c3_i     pen_i;

  while ( (msg_u = curl_multi_info_read(g_curl.mul_u, &pen_i)) ) {
    if ( CURLMSG_DONE == msg_u->msg ) {
      CURL*        ezy_u = msg_u->easy_handle;
      u3_curl_req* req_u = 0;
      long         cod_l = 0;          //  curl writes a C long here, not c3_l

      curl_easy_getinfo(ezy_u, CURLINFO_PRIVATE, (char**)&req_u);
      curl_easy_getinfo(ezy_u, CURLINFO_RESPONSE_CODE, &cod_l);

      {
        u3_lua_driver* dvr_u = req_u ? _live_get(req_u->dvr_uid) : 0;
        if ( dvr_u ) {
          lua_State* L = dvr_u->lua_u->lua;
          lua_rawgeti(L, LUA_REGISTRYINDEX, req_u->fn_r);
          lua_pushinteger(L, (lua_Integer)cod_l);
          lua_pushlstring(L, req_u->bod_c ? req_u->bod_c : "", req_u->bln_i);
          lua_pushlstring(L, req_u->hdr_c ? req_u->hdr_c : "", req_u->hln_i);
          if ( LUA_OK != lua_pcall(L, 3, 0, 0) ) {
            u3l_log("lua: %s: http error: %s", dvr_u->nam_c, lua_tostring(L, -1));
            lua_pop(L, 1);
          }
          luaL_unref(L, LUA_REGISTRYINDEX, req_u->fn_r);
        }
      }

      curl_multi_remove_handle(g_curl.mul_u, ezy_u);
      curl_easy_cleanup(ezy_u);
      if ( req_u ) {
        if ( req_u->shd_u ) curl_slist_free_all(req_u->shd_u);
        if ( req_u->bod_c ) c3_free(req_u->bod_c);
        if ( req_u->hdr_c ) c3_free(req_u->hdr_c);
        c3_free(req_u);
      }
    }
  }
}

static void
_curl_poll_cb(uv_poll_t* pol_u, c3_i sas_i, c3_i evt_i)
{
  u3_curl_ctx* ctx_u = pol_u->data;
  c3_i         flg_i = 0;
  c3_i         run_i;

  if ( sas_i < 0 )            flg_i |= CURL_CSELECT_ERR;
  if ( evt_i & UV_READABLE )  flg_i |= CURL_CSELECT_IN;
  if ( evt_i & UV_WRITABLE )  flg_i |= CURL_CSELECT_OUT;

  curl_multi_socket_action(g_curl.mul_u, ctx_u->sok_i, flg_i, &run_i);
  _curl_check_info();
}

static void
_curl_poll_close_cb(uv_handle_t* han_u)
{
  c3_free(han_u->data);
}

static c3_i
_curl_socket_cb(CURL* ezy_u, curl_socket_t sok_i, c3_i act_i,
                void* usr_v, void* sok_v)
{
  u3_curl_ctx* ctx_u = sok_v;

  switch ( act_i ) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT: {
      if ( !ctx_u ) {
        ctx_u = c3_calloc(sizeof(*ctx_u));
        ctx_u->sok_i = sok_i;
        uv_poll_init_socket(u3L, &ctx_u->pol_u, sok_i);
        ctx_u->pol_u.data = ctx_u;
        curl_multi_assign(g_curl.mul_u, sok_i, ctx_u);
      }
      {
        c3_i evt_i = ( (act_i & CURL_POLL_IN)  ? UV_READABLE : 0 )
                   | ( (act_i & CURL_POLL_OUT) ? UV_WRITABLE : 0 );
        uv_poll_start(&ctx_u->pol_u, evt_i, _curl_poll_cb);
      }
    } break;

    case CURL_POLL_REMOVE: {
      if ( ctx_u ) {
        uv_poll_stop(&ctx_u->pol_u);
        uv_close((uv_handle_t*)&ctx_u->pol_u, _curl_poll_close_cb);
        curl_multi_assign(g_curl.mul_u, sok_i, 0);
      }
    } break;
  }
  return 0;
}

static void
_curl_timer_cb(uv_timer_t* tim_u)
{
  c3_i run_i;
  curl_multi_socket_action(g_curl.mul_u, CURL_SOCKET_TIMEOUT, 0, &run_i);
  _curl_check_info();
}

static c3_i
_curl_timerfunc(CURLM* mul_u, long tim_l, void* usr_v)
{
  if ( tim_l < 0 ) {
    uv_timer_stop(&g_curl.tim_u);
  }
  else {
    uv_timer_start(&g_curl.tim_u, _curl_timer_cb,
                   (uint64_t)(tim_l ? tim_l : 1), 0);
  }
  return 0;
}

static void
_curl_accum(c3_c** buf_c, size_t* len_i, const c3_c* dat_c, size_t add_i)
{
  if ( !*buf_c ) {
    *buf_c = c3_malloc(add_i + 1);
  }
  else {
    *buf_c = c3_realloc(*buf_c, *len_i + add_i + 1);
  }
  memcpy(*buf_c + *len_i, dat_c, add_i);
  *len_i += add_i;
  (*buf_c)[*len_i] = 0;
}

static size_t
_curl_write_cb(c3_c* ptr_c, size_t siz_i, size_t nme_i, void* usr_v)
{
  u3_curl_req* req_u = usr_v;
  _curl_accum(&req_u->bod_c, &req_u->bln_i, ptr_c, siz_i * nme_i);
  return siz_i * nme_i;
}

static size_t
_curl_header_cb(c3_c* ptr_c, size_t siz_i, size_t nme_i, void* usr_v)
{
  u3_curl_req* req_u = usr_v;
  _curl_accum(&req_u->hdr_c, &req_u->hln_i, ptr_c, siz_i * nme_i);
  return siz_i * nme_i;
}

static c3_o
_curl_ensure(void)
{
  if ( c3y == g_curl.ini_o ) {
    return c3y;
  }
  if ( 0 != curl_global_init(CURL_GLOBAL_DEFAULT) ) {
    return c3n;
  }
  if ( !(g_curl.mul_u = curl_multi_init()) ) {
    return c3n;
  }
  curl_multi_setopt(g_curl.mul_u, CURLMOPT_SOCKETFUNCTION, _curl_socket_cb);
  curl_multi_setopt(g_curl.mul_u, CURLMOPT_TIMERFUNCTION, _curl_timerfunc);
  uv_timer_init(u3L, &g_curl.tim_u);
  g_curl.ini_o = c3y;
  return c3y;
}

/* ctx:http(method, url[, opts], fn): async HTTP(S) request.
**
**   opts (table, optional): { headers = {["K"]="V", ...}, body = "..." }.
**   fn(status, body, headers) is called on completion (status 0 on transport
**   failure). Follows redirects; 30s timeout.
*/
static int
_ctx_http(lua_State* L)
{
  u3_lua_driver* dvr_u = _check_ctx(L, 1);
  if ( !dvr_u->car_u.pir_u ) {
    return luaL_error(L, "ctx:http: unavailable without a pier");
  }
  const c3_c* met_c = luaL_checkstring(L, 2);
  const c3_c* url_c = luaL_checkstring(L, 3);
  luaL_checktype(L, 5, LUA_TFUNCTION);

  if ( c3n == _curl_ensure() ) {
    return luaL_error(L, "ctx:http: curl init failed");
  }

  u3_curl_req* req_u = c3_calloc(sizeof(*req_u));
  req_u->dvr_uid = dvr_u->uid_d;
  CURL* ezy_u = curl_easy_init();
  req_u->ezy_u = ezy_u;

  curl_easy_setopt(ezy_u, CURLOPT_URL, url_c);
  curl_easy_setopt(ezy_u, CURLOPT_CUSTOMREQUEST, met_c);
  curl_easy_setopt(ezy_u, CURLOPT_WRITEFUNCTION, _curl_write_cb);
  curl_easy_setopt(ezy_u, CURLOPT_WRITEDATA, req_u);
  curl_easy_setopt(ezy_u, CURLOPT_HEADERFUNCTION, _curl_header_cb);
  curl_easy_setopt(ezy_u, CURLOPT_HEADERDATA, req_u);
  curl_easy_setopt(ezy_u, CURLOPT_PRIVATE, req_u);
  curl_easy_setopt(ezy_u, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(ezy_u, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(ezy_u, CURLOPT_TIMEOUT, 30L);

  //  optional headers/body
  //
  if ( lua_istable(L, 4) ) {
    lua_getfield(L, 4, "headers");
    if ( lua_istable(L, -1) ) {
      lua_pushnil(L);
      while ( lua_next(L, -2) ) {                    //  key -2, value -1
        const c3_c* key_c = lua_tostring(L, -2);
        const c3_c* val_c = lua_tostring(L, -1);
        if ( key_c && val_c ) {
          c3_w  lin_w = (c3_w)(strlen(key_c) + 2 + strlen(val_c) + 1);
          c3_c* lin_c = c3_malloc(lin_w);
          snprintf(lin_c, lin_w, "%s: %s", key_c, val_c);
          req_u->shd_u = curl_slist_append(req_u->shd_u, lin_c);
          c3_free(lin_c);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 4, "body");
    if ( lua_isstring(L, -1) ) {
      size_t      bln_i;
      const c3_c* bod_c = lua_tolstring(L, -1, &bln_i);
      curl_easy_setopt(ezy_u, CURLOPT_POSTFIELDSIZE, (long)bln_i);
      curl_easy_setopt(ezy_u, CURLOPT_COPYPOSTFIELDS, bod_c);  //  curl copies
    }
    lua_pop(L, 1);
  }

  if ( req_u->shd_u ) {
    curl_easy_setopt(ezy_u, CURLOPT_HTTPHEADER, req_u->shd_u);
  }

  lua_pushvalue(L, 5);
  req_u->fn_r = luaL_ref(L, LUA_REGISTRYINDEX);

  curl_multi_add_handle(g_curl.mul_u, ezy_u);
  return 0;
}

static const luaL_Reg _ctx_methods[] = {
  { "log",       _ctx_log       },
  { "plan",      _ctx_plan      },
  { "scry",      _ctx_scry      },
  { "http",      _ctx_http      },
  { "wish",      _ctx_wish      },
  { "pier_path", _ctx_pier_path },
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

/*  ------------------------------------------------------------------------
 *  luv embedding: each driver state owns a private libuv loop (created by
 *  luaopen_luv) that we drive from the king's loop. luv's own loop_gc closes
 *  the driver's handles on lua_close -- so isolation and teardown are luv's
 *  job, not ours. The harness below is the only glue: it makes the king loop
 *  run the child loop whenever the child has fds ready or a timer due.
 *  ------------------------------------------------------------------------ */

typedef struct _u3_lua_emb {
  uv_loop_t*   sub_u;                   //  child loop (owned by luv/lua_State)
  uv_prepare_t pre_u;                   //  arms the timer to child's timeout
  uv_poll_t    pol_u;                   //  watches child's backend fd
  uv_timer_t   tim_u;                   //  wakes king for child timers
  uv_check_t   chk_u;                   //  drains child after each king poll
  c3_w         ref_w;                   //  open harness handles (for free)
} u3_lua_emb;

static void
_emb_run(u3_lua_emb* emb_u)
{
  uv_run(emb_u->sub_u, UV_RUN_NOWAIT);
}

static void _emb_timer_cb(uv_timer_t* h) { _emb_run(h->data); }
static void _emb_poll_cb(uv_poll_t* h, c3_i s, c3_i e) { _emb_run(h->data); }
static void _emb_check_cb(uv_check_t* h) { _emb_run(h->data); }

/* _emb_prep_cb(): before the king loop blocks, wake it when the child loop
**   next has a timer due (fd readiness is handled by the backend-fd poll).
*/
static void
_emb_prep_cb(uv_prepare_t* h)
{
  u3_lua_emb* emb_u = h->data;
  c3_i tim_i = uv_backend_timeout(emb_u->sub_u);
  if ( tim_i < 0 ) {
    uv_timer_stop(&emb_u->tim_u);
  }
  else {
    uv_timer_start(&emb_u->tim_u, _emb_timer_cb, (uint64_t)tim_i, 0);
  }
}

static void
_emb_close_cb(uv_handle_t* h)
{
  u3_lua_emb* emb_u = h->data;
  if ( 0 == --emb_u->ref_w ) {
    c3_free(emb_u);
  }
}

/* _lua_embed_luv(): open luv on its own loop and drive it from [u3L].
*/
static u3_lua_emb*
_lua_embed_luv(lua_State* L)
{
  //  luv creates and owns a private loop (we do NOT luv_set_loop), so its
  //  loop_gc will close this state's handles on lua_close.
  //
  luaL_requiref(L, "luv", luaopen_luv, 0);
  lua_setglobal(L, "uv");                            //  drivers use global `uv`

  //  remove loop-control escapes a driver must not call on the shared process
  //
  lua_getglobal(L, "uv");
  {
    static const char* ban_c[] = { "run", "stop", "loop_close", "loop_mode", 0 };
    for ( c3_w i_w = 0; ban_c[i_w]; i_w++ ) {
      lua_pushnil(L);
      lua_setfield(L, -2, ban_c[i_w]);
    }
  }
  lua_pop(L, 1);

  u3_lua_emb* emb_u = c3_calloc(sizeof(*emb_u));
  emb_u->sub_u = luv_loop(L);
  emb_u->ref_w = 4;

  uv_prepare_init(u3L, &emb_u->pre_u); emb_u->pre_u.data = emb_u;
  uv_timer_init(u3L, &emb_u->tim_u);   emb_u->tim_u.data = emb_u;
  uv_check_init(u3L, &emb_u->chk_u);   emb_u->chk_u.data = emb_u;
  uv_poll_init(u3L, &emb_u->pol_u, uv_backend_fd(emb_u->sub_u));
  emb_u->pol_u.data = emb_u;

  uv_prepare_start(&emb_u->pre_u, _emb_prep_cb);
  uv_check_start(&emb_u->chk_u, _emb_check_cb);
  uv_poll_start(&emb_u->pol_u, UV_READABLE, _emb_poll_cb);
  return emb_u;
}

/* _lua_unembed_luv(): stop+close the harness; child handles close on lua_close.
*/
static void
_lua_unembed_luv(u3_lua_emb* emb_u)
{
  uv_prepare_stop(&emb_u->pre_u);
  uv_timer_stop(&emb_u->tim_u);
  uv_check_stop(&emb_u->chk_u);
  uv_poll_stop(&emb_u->pol_u);
  uv_close((uv_handle_t*)&emb_u->pre_u, _emb_close_cb);
  uv_close((uv_handle_t*)&emb_u->tim_u, _emb_close_cb);
  uv_close((uv_handle_t*)&emb_u->chk_u, _emb_close_cb);
  uv_close((uv_handle_t*)&emb_u->pol_u, _emb_close_cb);
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

  {
    u3_lua* lua_u = c3_calloc(sizeof(*lua_u));
    lua_u->lua   = L;
    lua_u->pir_u = pir_u;

    //  embed luv only when a real king loop exists (tests run without u3L,
    //  sometimes with a fake pier, so guard on the loop itself)
    //
    if ( pir_u && u3L ) {
      lua_u->emb_u = _lua_embed_luv(L);
    }
    return lua_u;
  }
}

void
u3_lua_halt(u3_lua* lua_u)
{
  if ( !lua_u ) {
    return;
  }
  if ( lua_u->emb_u ) {
    _lua_unembed_luv((u3_lua_emb*)lua_u->emb_u);     //  stop driving child loop
  }
  if ( lua_u->lua ) {
    lua_close(lua_u->lua);               //  luv loop_gc closes child handles
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

  _live_del(dvr_u->uid_d);                           //  scry/http callbacks drop
  luaL_unref(L, LUA_REGISTRYINDEX, dvr_u->ctx_r);
  luaL_unref(L, LUA_REGISTRYINDEX, dvr_u->tab_r);
  c3_free(dvr_u->nam_c);
  c3_free(dvr_u->src_c);
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

  //  capture the file's mtime (ns) for incremental-reload change detection
  //
  c3_d mod_d = 0;
  {
    struct stat buf_u;
    if ( 0 == stat(pax_c, &buf_u) ) {
      mod_d = ((c3_d)buf_u.st_mtim.tv_sec * 1000000000ULL) + buf_u.st_mtim.tv_nsec;
    }
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
  dvr_u->src_c = _lua_strdup(nam_c);
  dvr_u->mod_d = mod_d;
  dvr_u->uid_d = ++g_uid_d;
  dvr_u->tab_r = tab_r;
  dvr_u->ctx_r = _make_ctx(L, dvr_u);
  _live_add(dvr_u);

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

/* _lua_file_mtime(): mtime (ns) of <dir_c>/<nam_c>, or 0.
*/
static c3_d
_lua_file_mtime(const c3_c* dir_c, const c3_c* nam_c)
{
  c3_c*       pax_c = _lua_join(dir_c, nam_c);
  struct stat buf_u;
  c3_d        mod_d = 0;
  if ( 0 == stat(pax_c, &buf_u) ) {
    mod_d = ((c3_d)buf_u.st_mtim.tv_sec * 1000000000ULL) + buf_u.st_mtim.tv_nsec;
  }
  c3_free(pax_c);
  return mod_d;
}

/* _lua_chain_insert(): splice [dvr_u] into the Lua block after [anc_u], keeping
**   the block sorted by ascending priority (ties append).
*/
static void
_lua_chain_insert(u3_auto* anc_u, u3_lua_driver* dvr_u)
{
  u3_auto* pre_u = anc_u;
  while ( pre_u->nex_u
       && (c3__lua == pre_u->nex_u->nam_m)
       && (((u3_lua_driver*)pre_u->nex_u)->pri_w <= dvr_u->pri_w) ) {
    pre_u = pre_u->nex_u;
  }
  dvr_u->car_u.nex_u = pre_u->nex_u;
  pre_u->nex_u = &dvr_u->car_u;
}

/* _lua_chain_remove(): unlink [tgt_u] from the chain and exit just that driver.
*/
static void
_lua_chain_remove(u3_auto* anc_u, u3_auto* tgt_u)
{
  u3_auto* pre_u = anc_u;
  while ( pre_u->nex_u && (pre_u->nex_u != tgt_u) ) {
    pre_u = pre_u->nex_u;
  }
  if ( pre_u->nex_u == tgt_u ) {
    pre_u->nex_u = tgt_u->nex_u;       //  unlink
    tgt_u->nex_u = 0;                  //  null-terminate the single node
    u3_auto_exit(tgt_u);              //  exit + free (closes its luv loop)
  }
}

/* _lua_reload(): incrementally reconcile the loaded Lua drivers with disk.
**
**   Diffs $pier/lua/ against the currently-loaded drivers (by filename + mtime)
**   and only touches what changed: a new file is loaded and spliced at its
**   priority slot; a deleted file's driver is exited; an edited file's driver
**   is replaced. Unchanged drivers keep running -- their sockets, timers, and
**   state are left alone.
**
**   Runs from the (debounced) watcher on the libuv loop, never during effect
**   dispatch, so chain surgery is reentrancy-safe. (A driver with an Arvo event
**   in flight at the instant *it* is replaced/removed remains the one edge, now
**   scoped to just that driver.)
*/
#define U3_LUA_MAX_DRIVERS 256

static void
_lua_reload(u3_lua_load* lod_u)
{
  u3_auto* anc_u = lod_u->anc_u;

  //  snapshot the current Lua block
  //
  u3_lua_driver* cur_u[U3_LUA_MAX_DRIVERS];
  c3_o           keep_o[U3_LUA_MAX_DRIVERS];
  c3_w           cnt_w = 0;
  for ( u3_auto* c = anc_u->nex_u;
        c && (c3__lua == c->nam_m) && (cnt_w < U3_LUA_MAX_DRIVERS);
        c = c->nex_u ) {
    cur_u[cnt_w]  = (u3_lua_driver*)c;
    keep_o[cnt_w] = c3n;
    cnt_w++;
  }

  u3_dire* fol_u = u3_foil_folder(lod_u->dir_c);
  if ( !fol_u ) {
    return;
  }

  c3_w add_w = 0, chg_w = 0, del_w = 0;

  //  walk the directory: add new files, replace changed ones, keep the rest
  //
  for ( u3_dent* den_u = fol_u->all_u; den_u; den_u = den_u->nex_u ) {
    if ( c3n == _lua_is_script(den_u->nam_c) ) {
      continue;
    }
    c3_d mod_d = _lua_file_mtime(lod_u->dir_c, den_u->nam_c);

    u3_lua_driver* mat_u = 0;
    c3_w           mat_w = 0;
    for ( c3_w i_w = 0; i_w < cnt_w; i_w++ ) {
      if ( cur_u[i_w]->src_c && (0 == strcmp(cur_u[i_w]->src_c, den_u->nam_c)) ) {
        mat_u = cur_u[i_w];
        mat_w = i_w;
        break;
      }
    }

    if ( mat_u && (mat_u->mod_d == mod_d) ) {
      keep_o[mat_w] = c3y;             //  unchanged -> leave it running
      continue;
    }

    //  new file, or edited (mat_u kept marked for removal below)
    //
    u3_lua_driver* new_u = _lua_load_one(lod_u->pir_u, lod_u->dir_c, den_u->nam_c);
    if ( new_u ) {
      _lua_chain_insert(anc_u, new_u);
      new_u->car_u.io.talk_f(&new_u->car_u);   //  start just this driver
      if ( mat_u ) { chg_w++; } else { add_w++; }
    }
    else if ( mat_u ) {
      keep_o[mat_w] = c3y;             //  reload failed; keep the old one running
    }
  }
  u3_dire_free(fol_u);

  //  remove drivers whose file is gone or was replaced
  //
  for ( c3_w i_w = 0; i_w < cnt_w; i_w++ ) {
    if ( c3n == keep_o[i_w] ) {
      _lua_chain_remove(anc_u, &cur_u[i_w]->car_u);
      del_w++;
    }
  }

  if ( add_w || chg_w || del_w ) {
    u3l_log("lua: reload: +%u added, ~%u changed, -%u removed",
            add_w, chg_w, del_w - chg_w);
  }
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
