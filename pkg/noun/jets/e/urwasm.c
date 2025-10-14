/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"

// #define URWASM_SUBROAD
#define URWASM_STATEFUL

#define ONCE_CTX            63
#define RUN_CTX             7

#define AX_RUNNABLE         374
#define AX_ARROWS           1502

#define AX_CALL             20
#define AX_MEMREAD          383
#define AX_MEMWRITE         94
#define AX_CALL_EXT         375
#define AX_GLOBAL_SET       4
#define AX_GLOBAL_GET       22
#define AX_MEM_SIZE         186
#define AX_MEM_GROW         381
#define AX_GET_ACC          374
#define AX_SET_ACC          92
#define AX_GET_ALL_GLOB     43
#define AX_SET_ALL_GLOB     380

#define AX_TRY              43
#define AX_CATCH            4
#define AX_RETURN           20
#define AX_FAIL             47

#define ARROW_CTX           511
#define MONAD_CTX           127

#define arr_sam             62
#define arr_sam_2           124
#define arr_sam_3           125
#define arr_sam_6           250
#define arr_sam_7           251

#define seed_module         2
#define seed_past           6
#define seed_shop           14
#define seed_import         15

#define uw__lia             c3_s3('l', 'i', 'a')

#define uw_lia_run_version  1

#define ERR(string)         ("\r\n\033[31m>>> " string "\033[0m\r\n")
#define WUT(string)         ("\r\n\033[33m>>  " string "\033[0m\r\n")
#define DBG(string)         ("\r\n" string "\r\n")

#define KICK1(TRAP)         uw_kick_nock(TRAP, 2)
#define KICK2(TRAP)         KICK1(KICK1(TRAP))

// [a b c d e f g h]
static inline u3_noun
uw_octo(u3_noun a,
  u3_noun b,
  u3_noun c,
  u3_noun d,
  u3_noun e,
  u3_noun f,
  u3_noun g,
  u3_noun h)
{
  return u3nc(a, u3nq(b, c, d, u3nq(e, f, g, h)));
}

// kick by nock. axe RETAINED (ignore if direct)
static u3_noun
uw_kick_nock(u3_noun cor, u3_noun axe)
{
  u3_noun fol = u3x_at(axe, cor);
  return u3n_nock_on(cor, u3k(fol));
}

// slam by nock
static u3_noun
uw_slam_nock(u3_noun gat, u3_noun sam)
{
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));
  u3z(gat);
  return uw_kick_nock(cor, 2);
}

static u3_noun
uw_slam_check(u3_noun gat, u3_noun sam, c3_t is_stateful)
{
  u3_noun bat = u3k(u3h(gat));
  u3_noun cor = u3nc(u3k(bat), u3nc(sam, u3k(u3t(u3t(gat)))));
  u3z(gat);

  if (!is_stateful)
  {
    return u3n_nock_on(cor, bat);
  }
  else
  {
    u3_noun ton = u3n_nock_an(cor, bat);
    
    u3_noun tag, pro;
    if (c3n == u3r_cell(ton, &tag, &pro))
    {
      return u3m_bail(c3__fail);
    }
    if (0 == tag)
    {
      u3k(pro);
      u3z(ton);
      return pro;
    }
    else if (2 == tag)
    {
      return u3m_bail(c3__exit);
    }
    else
    {
      return u3m_bail(c3__fail);
    }
  }
}

static inline void
_push_list(u3_noun som, u3_noun *lit)
{
  if (u3_none == *lit)
  {
    u3z(som);
  }
  else
  {
    *lit = u3nc(som, *lit);
  }
}

static inline u3_weak
_pop_list(u3_weak *lit)
{
  if (u3_none == *lit)
  {
    return u3_none;
  }
  u3_noun hed, tel;
  if (c3n == u3r_cell(*lit, &hed, &tel))
  {
    return u3m_bail(c3__fail);
  }
  u3k(hed);
  u3k(tel);
  u3z(*lit);
  *lit = tel;
  return hed;
}

static const M3Result UrwasmArrowExit = "An imported arrow returned %2";

static const c3_m uw_run_m = uw__lia + c3__run + uw_lia_run_version;

static_assert(
  (c3y == u3a_is_cat(uw_run_m)),
  "u3we_run key tag must be a direct atom"
);

typedef struct {
  u3_noun call_bat;
  u3_noun memread_bat;
  u3_noun memwrite_bat;
  u3_noun call_ext_bat;
  u3_noun try_bat;
  u3_noun catch_bat;
  u3_noun return_bat;
  u3_noun fail_bat;
  u3_noun global_set_bat;
  u3_noun global_get_bat;
  u3_noun mem_grow_bat;
  u3_noun mem_size_bat;
  u3_noun get_acc_bat;
  u3_noun set_acc_bat;
  u3_noun get_all_glob_bat;
  u3_noun set_all_glob_bat;
//
  u3_noun call_ctx;
  u3_noun memread_ctx;
  u3_noun memwrite_ctx;
  u3_noun global_set_ctx;
  u3_noun global_get_ctx;
  u3_noun mem_grow_ctx;
  u3_noun mem_size_ctx;
  u3_noun get_all_glob_ctx;
  u3_noun set_all_glob_ctx;
} match_data_struct;

// memory arena with exponential growth
typedef struct {
  c3_w      siz_w;  // size in bytes
  c3_y      pad_y;  // alignment padding
  c3_t      ini_t;  // already initialized
  u3i_slab  sab_u;  // associated slab
  c3_y*     buf_y;  // allocated buffer
  c3_y*     nex_y;  // next allocation
  c3_y*     end_y;  // end of arena
  jmp_buf*  esc_u;  // escape buffer
} uw_arena;

typedef struct {
  IM3Module wasm_module;    // p
  u3_noun lia_shop;         // q,   transferred
  u3_noun acc;              // p.r, transferred
  u3_noun map;              // q.r, retained
  match_data_struct* match;
  u3_noun arrow_yil;        // transferred
  u3_noun susp_list;        // transferred
  u3_noun resolution;       // resolved %1 block, transferred
  uw_arena box_arena;
  uw_arena code_arena;
  u3_noun yil_previous;     // transferred
  u3_noun queue;            // transferred
  c3_t is_stateful;
} lia_state;

typedef enum {
  west_call,
  west_call_ext,
  west_try,
  west_catch_try,
  west_catch_err,
  west_link_wasm,
} wasm3_ext_suspend_tag;

typedef enum {
  lst_call      = 0,
  // lst_call_ext  = 1,  // not necessary
  lst_try       = 2,
  lst_catch_try = 3,
  lst_catch_err = 4,
  lst_link_wasm = 5,
} lia_suspend_tag;

static void
_uw_arena_init_size(uw_arena* ren_u, c3_w siz_w)
{
  ren_u->siz_w = siz_w;
  u3i_slab_init(&ren_u->sab_u, 3, siz_w + 12);  // size + max padding
  ren_u->buf_y = ren_u->nex_y = c3_align(ren_u->sab_u.buf_y, 16, C3_ALGHI);
  ren_u->end_y = ren_u->buf_y + ren_u->siz_w;
  c3_y pad_y = ren_u->buf_y - ren_u->sab_u.buf_y;
  if (pad_y > 12)
  {
    u3m_bail(c3__fail);
  }
  ren_u->pad_y = pad_y;
  ren_u->ini_t = 1;
}

static void
_uw_arena_init(uw_arena* ren_u)
{
  _uw_arena_init_size(ren_u, (c3_w)1 << 23);
}

static void
_uw_arena_grow(uw_arena* ren_u)
{
  if (!ren_u->ini_t)
  {
    u3m_bail(c3__fail);
  }
  c3_w new_w = ren_u->siz_w * 2;
  if (new_w / 2 != ren_u->siz_w)
  {
    u3m_bail(c3__fail);
  }
  ren_u->siz_w = new_w;

  u3i_slab_free(&ren_u->sab_u);

  u3i_slab_init(&ren_u->sab_u, 3, new_w + 12);  // size + max padding
  ren_u->buf_y = ren_u->nex_y = c3_align(ren_u->sab_u.buf_y, 16, C3_ALGHI);
  ren_u->end_y = ren_u->nex_y + new_w;
  c3_y pad_y = ren_u->nex_y - ren_u->sab_u.buf_y;
  if (pad_y > 12)
  {
    u3m_bail(c3__fail);
  }
  ren_u->pad_y = pad_y;
}

static void
_uw_arena_reset(uw_arena* ren_u)
{
  if (!ren_u->ini_t)
  {
    u3m_bail(c3__fail);
  }
  ren_u->nex_y = ren_u->buf_y;
  memset(ren_u->buf_y, 0, (size_t)ren_u->siz_w);
}

static void
_uw_arena_free(uw_arena* ren_u)
{
  if (!ren_u->ini_t)
  {
    u3m_bail(c3__fail);
  }
  u3i_slab_free(&ren_u->sab_u);
  ren_u->ini_t = 0;
}

// Code page allocation: simple bump allocator for non-growing objects,
// i.e. code pages
// save allocation length for realloc
// CodeArena->esc_u MUST be initialized by the caller to handle OOM
//
static uw_arena* CodeArena;

static void*
_calloc_code(size_t num_i, size_t len_i)
{
  if (!CodeArena->ini_t)
  {
    u3m_bail(c3__fail);
  }

  void* lag_v = CodeArena->nex_y;
  
  size_t byt_i = num_i * len_i;
  if (byt_i / len_i != num_i)
  {
    u3m_bail(c3__fail);
  }

  if (byt_i >= UINT64_MAX - 16)
  {
    u3m_bail(c3__fail);
  }
  c3_d byt_d = byt_i + 16; // c3_d for length + alignment padding

  c3_y* nex_y = CodeArena->nex_y + byt_d;
  nex_y = c3_align(nex_y, 16, C3_ALGHI);
  
  if (nex_y >= CodeArena->end_y)
  { // OOM, jump out to increase the arena size and try again
    _longjmp(*CodeArena->esc_u, c3__code);
  }

  *((c3_d*)lag_v) = byt_d - 16;  // corruption check
  *((c3_d*)lag_v + 1) = byt_d - 16;

  CodeArena->nex_y = nex_y;
  return ((c3_d*)lag_v + 2);
}

static void*
_realloc_code(void* lag_v, size_t len_i)
{
  if (!CodeArena->ini_t)
  {
    u3m_bail(c3__fail);
  }
  if (!lag_v)
  {
    return _calloc_code(len_i, 1);
  }
  c3_d old1_d = *((c3_d*)lag_v - 1);
  c3_d old2_d = *((c3_d*)lag_v - 2);
  if (old1_d != old2_d)
  {
    u3m_bail(c3__fail);
  }
  if (len_i >= UINT64_MAX)
  {
    u3m_bail(c3__fail);
  }
  c3_d len_d = len_i;
  void* new_v = _calloc_code(len_d, 1);
  memcpy(new_v, lag_v, c3_min(len_d, old1_d));

  return new_v;
}

static void
_free_code(void* lag_v)
{
  if (!CodeArena->ini_t)
  {
    u3m_bail(c3__fail);
  }
  // noop
}

// Struct/array allocation: [len_d cap_d data]
// BoxArena->esc_u MUST be initialized by the caller to handle OOM
//
static uw_arena* BoxArena;

//  allocate with capacity
//  the allocated buffer 
static void*
_malloc_box_cap(c3_d len_d, c3_d cap_d)
{
  if (!BoxArena->ini_t)
  {
    u3m_bail(c3__fail);
  }

  void* lag_v = BoxArena->nex_y;

  if (cap_d >= UINT64_MAX - 16)
  {
    u3m_bail(c3__fail);
  }
  c3_d pac_d = cap_d + 16; // c3_d for length + capacity

  c3_y* nex_y = BoxArena->nex_y + pac_d;
  nex_y = c3_align(nex_y, 16, C3_ALGHI);
  
  if (nex_y >= BoxArena->end_y)
  { // OOM, jump out to increase the arena size and try again
    _longjmp(*BoxArena->esc_u, c3__box);
  }

  *((c3_d*)lag_v) = len_d;
  *((c3_d*)lag_v + 1) = cap_d;

  BoxArena->nex_y = nex_y;
  return ((c3_d*)lag_v + 2);
}

static void*
_calloc_box(size_t num_i, size_t len_i)
{
  size_t byt_i = num_i * len_i;
  if (byt_i / len_i != num_i)
  {
    u3m_bail(c3__fail);
  }
  if (byt_i > UINT64_MAX - 16)
  {
    u3m_bail(c3__fail);
  }
  c3_d byt_d = byt_i;
  return _malloc_box_cap(byt_d, byt_d);
}

static void*
_realloc_box(void* lag_v, size_t len_i)
{
  if (!BoxArena->ini_t)
  {
    u3m_bail(c3__fail);
  }
  if (!lag_v)
  {
    return _calloc_box(len_i, 1);
  }
  c3_d old_d = *((c3_d*)lag_v - 2);
  c3_d cap_d = *((c3_d*)lag_v - 1);
  if (len_i >= UINT64_MAX)
  {
    u3m_bail(c3__fail);
  }
  c3_d len_d = len_i;
  if (len_d <= cap_d)
  {
    *((c3_d*)lag_v - 2) = len_d;
    return lag_v;
  }

  // while (cap_d <= len_d)
  // {
  //   cap_d *= 2;
  // }
  cap_d <<= c3_bits_dabl(len_d) - c3_bits_dabl(cap_d);
  cap_d <<= (cap_d <= len_d);
  
  //  overflow check
  if (cap_d <= len_d)
    u3m_bail(c3__fail);

  void* new_v = _malloc_box_cap(len_d, cap_d);
  memcpy(new_v, lag_v, old_d);

  return new_v;
}

static void
_free_box(void* lag_v)
{
  if (!BoxArena->ini_t)
  {
    u3m_bail(c3__fail);
  }
  // noop
}

// bailing allocator to prevent wasm3 from touching the arenas

static void*
_calloc_bail(size_t num_i, size_t len_i)
{
  u3m_bail(c3__fail);
}

static void*
_realloc_bail(void* lag_v, size_t len_i)
{
  u3m_bail(c3__fail);
}

static void
_free_bail(void* lag_v)
{
  u3m_bail(c3__fail);
}


static u3_noun
_atoms_from_stack(void** valptrs, c3_w n, c3_y* types)
{
  u3_noun out = u3_nul;
  while (n--)
  {
    switch (types[n])
    {  // TODO 64 bit vere
      case c_m3Type_i32:
      case c_m3Type_f32:
      {
        out = u3nc(u3i_word(*(c3_w*)valptrs[n]), out);
        break;
      }
      case c_m3Type_i64:
      case c_m3Type_f64:
      {
        out = u3nc(u3i_chub(*(c3_d*)valptrs[n]), out);
        break;
      }
      default:
      {
        return u3m_bail(c3__fail);
      }
    }
  }
  return out;
}

//  RETAIN argument
static c3_o
_atoms_to_stack(u3_noun atoms, void** valptrs, c3_w n, c3_y* types)
{
  for (c3_w i = 0; i < n; i++)
  {
    if (c3y == u3ud(atoms))
    {
      return c3n;
    }
    u3_noun atom;
    u3x_cell(atoms, &atom, &atoms);
    if (c3n == u3ud(atom))
    {
      return u3m_bail(c3__fail);
    }
    switch (types[i])
    {
      case c_m3Type_i32:
      case c_m3Type_f32:
      {
        *(c3_w*)valptrs[i] = u3r_word(0, atom);
        break;
      }
      case c_m3Type_i64:
      case c_m3Type_f64:
      {
        *(c3_d*)valptrs[i] = u3r_chub(0, atom);
        break;
      }
      default:
      {
        return u3m_bail(c3__fail);
      }
    }
  }
  return __(u3_nul == atoms);
}

static u3_noun
_coins_from_stack(void** valptrs, c3_w n, c3_y* types)
{
  u3_noun out = u3_nul;
  while (n--)
  {
    switch (types[n])
    {  // TODO 64 bit vere
      case c_m3Type_i32:
      {
        out = u3nc(u3nc(c3__i32, u3i_word(*(c3_w*)valptrs[n])), out);
        break;
      }
      case c_m3Type_i64:
      {
        out = u3nc(u3nc(c3__i64, u3i_chub(*(c3_d*)valptrs[n])), out);
        break;
      }
      case c_m3Type_f32:
      {
        out = u3nc(u3nc(c3__f32, u3i_word(*(c3_w*)valptrs[n])), out);
        break;
      }
      case c_m3Type_f64:
      {
        out = u3nc(u3nc(c3__f64, u3i_chub(*(c3_d*)valptrs[n])), out);
        break;
      }
      default:
      {
        return u3m_bail(c3__fail);
      }
    }
  }
  return out;
}

//  RETAIN argument
static c3_o
_coins_to_stack(u3_noun coins, void** valptrs, c3_w n, c3_y* types)
{
  for (c3_w i = 0; i < n; i++)
  {
    if (c3y == u3ud(coins))
    {
      return c3n;
    }
    u3_noun coin;
    u3x_cell(coins, &coin, &coins);
    if (c3y == u3ud(coin))
    {
      return u3m_bail(c3__fail);
    }
    u3_noun tag, value;
    u3x_cell(coin, &tag, &value);
    if (c3n == u3ud(value))
    {
      return u3m_bail(c3__fail);
    }
    switch (types[i])
    {
      case c_m3Type_i32:
      {
        if (c3__i32 != tag)
        {
          return c3n;
        }
        *(c3_w*)valptrs[i] = u3r_word(0, value);
        break;
      }
      case c_m3Type_i64:
      {
        if (c3__i64 != tag)
        {
          return c3n;
        }
        *(c3_d*)valptrs[i] = u3r_chub(0, value);
        break;
      }
      case c_m3Type_f32:
      {
        if (c3__f32 != tag)
        {
          return c3n;
        }
        *(c3_w*)valptrs[i] = u3r_word(0, value);
        break;
      }
      case c_m3Type_f64:
      {
        if (c3__f64 != tag)
        {
          return c3n;
        }
        *(c3_d*)valptrs[i] = u3r_chub(0, value);
        break;
      }
      default:
      {
        return u3m_bail(c3__fail);
      }
    }
  }
  return __(u3_nul == coins);
}

static c3_t
_deterministic_trap(M3Result result)
{
  return ( result == m3Err_trapOutOfBoundsMemoryAccess
        || result == m3Err_trapDivisionByZero
        || result == m3Err_trapIntegerOverflow
        || result == m3Err_trapIntegerConversion
        || result == m3Err_trapIndirectCallTypeMismatch
        || result == m3Err_trapTableIndexOutOfRange
        || result == m3Err_trapTableElementIsNull
        || result == UrwasmArrowExit
  );
}

static u3_noun
_reduce_monad(u3_noun monad, lia_state* sat_u)
{
  u3_noun monad_bat = u3h(monad);
  if (c3y == u3r_sing(monad_bat, sat_u->match->call_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->call_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  call
    u3_atom name = u3x_atom(u3at(arr_sam_2, monad));
    u3_noun args = u3at(arr_sam_3, monad);

    c3_w met_w = u3r_met(3, name);
    c3_c* name_c = u3a_malloc(met_w + 1);
    u3r_bytes(0, met_w, (c3_y*)name_c, name);
    name_c[met_w] = 0;

    M3Result result;

    IM3Function f;
    result = m3_FindFunction(&f, sat_u->wasm_module->runtime, name_c);

    if (result)
    {
      fprintf(stderr, ERR("function %s search error: %s"), name_c, result);
      return u3m_bail(c3__fail);
    }

    c3_w n_in  = f->funcType->numArgs;
    c3_w n_out = f->funcType->numRets;
    c3_y* types = f->funcType->types;

    c3_d *vals_in = u3a_calloc(n_in, sizeof(c3_d));
    void **valptrs_in = u3a_calloc(n_in, sizeof(void*));
    for (c3_w i = 0; i < n_in; i++)
    {
      valptrs_in[i] = &vals_in[i];
    }

    c3_d *vals_out = u3a_calloc(n_out, sizeof(c3_d));
    void **valptrs_out = u3a_calloc(n_out, sizeof(void*));
    for (c3_w i = 0; i < n_out; i++)
    {
      valptrs_out[i] = &vals_out[i];
    }

    if (c3n == _atoms_to_stack(args, valptrs_in, n_in, (types+n_out)))
    {
      fprintf(stderr, ERR("function %s wrong number of args"), name_c);
      return u3m_bail(c3__fail);
    }

    c3_w edge_1 = sat_u->wasm_module->runtime->edge_suspend;

    // printf("\r\n\r\n invoke %s\r\n\r\n", name_c);

    { // push on suspend stacks
      c3_d f_idx_d = f - sat_u->wasm_module->functions;
      m3_SuspendStackPush64(sat_u->wasm_module->runtime, f_idx_d);
      m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_call);
      m3_SuspendStackPushExtTag(sat_u->wasm_module->runtime);
      _push_list(
        u3nc(lst_call, u3k(name)),
        &sat_u->susp_list
      );
    }

    M3Result result_call = m3_Call(f, n_in, (const void**)valptrs_in);
    // printf("\r\n done %s\r\n", name_c);

    if (result_call != m3Err_ComputationBlock
      && result_call != m3Err_SuspensionError)
    { // pop suspend stacks
      m3_SuspendStackPopExtTag(sat_u->wasm_module->runtime);
      c3_d tag;
      m3_SuspendStackPop64(sat_u->wasm_module->runtime, &tag);
      if (tag != -1 && tag != west_call)
      {
        printf(ERR("call tag mismatch: %"PRIc3_d), tag);
        return u3m_bail(c3__fail);
      }
      m3_SuspendStackPop64(sat_u->wasm_module->runtime, NULL);
      u3_noun frame = _pop_list(&sat_u->susp_list);
      if (u3_none != frame && lst_call != u3h(frame))
      {
        printf(ERR("wrong frame: call"));
        return u3m_bail(c3__fail);
      }
      u3z(frame);
    }

    u3_noun yil;
    if (result_call == m3Err_ComputationBlock)
    {
      yil = sat_u->arrow_yil;
      sat_u->arrow_yil = u3_none;
      if (yil == u3_none)
      {
        return u3m_bail(c3__fail);
      }
    }
    else if (_deterministic_trap(result_call))
    {
      fprintf(stderr, WUT("%s call trapped: %s"), name_c, result_call);
      yil = u3nc(2, 0);
    }
    else if (result_call == m3Err_functionImportMissing)
    {
      return u3m_bail(c3__exit);
    }
    else if (result_call)
    {
      fprintf(stderr, ERR("%s call failed: %s"), name_c, result_call);
      return u3m_bail(c3__fail);
    }
    else
    {
      result = m3_GetResults(f, n_out, (const void**)valptrs_out);
      if (result)
      {
        fprintf(stderr, ERR("function %s failed to get results"), name_c);
        return u3m_bail(c3__fail);
      }
      yil = u3nc(0, _atoms_from_stack(valptrs_out, n_out, types));
    }

    c3_w edge_2 = sat_u->wasm_module->runtime->edge_suspend;
    if (edge_1 != edge_2 && !result_call)
    {
      fprintf(stderr, ERR("imbalanced suspension stack on succesfull return: %d vs %d"), edge_1, edge_2);
      return u3m_bail(c3__fail);
    }

    u3a_free(name_c);
    u3a_free(vals_in);
    u3a_free(valptrs_in);
    u3a_free(vals_out);
    u3a_free(valptrs_out);
    u3z(monad);

    return yil;
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->memread_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->memread_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memread
    u3_atom ptr = u3x_atom(u3at(arr_sam_2, monad));
    u3_noun len = u3at(arr_sam_3, monad);

    c3_w ptr_w = u3r_word(0, ptr);
    c3_l len_l = (c3y == u3a_is_cat(len)) ? len : u3m_bail(c3__fail);
    c3_w len_buf_w;
    c3_y* buf_y = m3_GetMemory(sat_u->wasm_module->runtime, &len_buf_w, 0);

    if (buf_y == NULL)
    {
      fprintf(stderr, ERR("memread failed to get memory"));
      return u3m_bail(c3__fail);
    }

    if (ptr_w + len_l > len_buf_w)
    {
      fprintf(stderr, ERR("memread out of bounds"));
      return u3m_bail(c3__fail);
    }

    u3z(monad);
    return u3nt(0, len_l, u3i_bytes(len_l, (buf_y + ptr_w)));
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->memwrite_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->memwrite_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memwrite
    u3_atom ptr = u3x_atom(u3at(arr_sam_2, monad));
    u3_noun len = u3at(arr_sam_6, monad);
    u3_noun src = u3at(arr_sam_7, monad);

    c3_w ptr_w = u3r_word(0, ptr);
    c3_l len_l = (c3y == u3a_is_cat(len)) ? len : u3m_bail(c3__fail);

    c3_w len_buf_w;
    c3_y* buf_y = m3_GetMemory(sat_u->wasm_module->runtime, &len_buf_w, 0);

    if (buf_y == NULL)
    {
      fprintf(stderr, ERR("memwrite failed to get memory"));
      return u3m_bail(c3__fail);
    }

    if (ptr_w + len_l > len_buf_w)
    {
      fprintf(stderr, ERR("memwrite out of bounds"));
      return u3m_bail(c3__fail);
    }

    u3r_bytes(0, len_l, (buf_y + ptr_w), u3x_atom(src));
    
    u3z(monad);
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->call_ext_bat))
  {
    //  call-ext
    if (u3_nul == sat_u->lia_shop)
    {
      // Suspended computation will have exactly one blocking point, which
      // must be at the top of the stack. You are at this point.
      // There is no useful info to be saved here, name/args are not enough
      // to qualify the external call, which can and will be nondeterministic
      // (like all IO in urbit)
      //
      // On wasm3 side op_CallRaw will store the information about the
      // called function. It shall be the top frame of the suspension stack,
      // since only Lia can block, so wasm3 has to call Lia to get blocked.
      //
      // A frame is pushed in wasm3 to trigger the callback and signal to
      // _apply_diff that the computation is blocked
      //
      m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_call_ext);
      m3_SuspendStackPushExtTag(sat_u->wasm_module->runtime);

      u3_noun name = u3at(arr_sam_2, monad);
      u3_noun args = u3at(arr_sam_3, monad);

      u3_noun yil = u3nt(1, u3k(name), u3k(args));
      u3z(monad);
      return yil;
    }
    else
    {
      u3z(monad);

      u3_noun lia_buy, tel;
      u3x_cell(sat_u->lia_shop, &lia_buy, &tel);
      u3_noun yil = u3nc(0, u3k(lia_buy));
      u3k(tel);
      u3z(sat_u->lia_shop);
      sat_u->lia_shop = tel;
      return yil;
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->try_bat))
  {
    //  try
    u3_noun monad_b = u3at(60, monad);
    u3_noun cont = u3at(61, monad);
    u3_weak yil;
    u3_noun monad_cont;
    { // push on suspend stacks
      m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_try);
      m3_SuspendStackPushExtTag(sat_u->wasm_module->runtime);
      _push_list(u3nc(lst_try, u3k(cont)), &sat_u->susp_list);
    }
    {
      yil = _reduce_monad(u3k(monad_b), sat_u);

      if (1 != u3h(yil))
      { // pop suspend stacks
        m3_SuspendStackPopExtTag(sat_u->wasm_module->runtime);
        c3_d tag;
        m3_SuspendStackPop64(sat_u->wasm_module->runtime, &tag);
        if (tag != -1 && tag != west_try)
        {
          printf(ERR("try tag mismatch: %"PRIc3_d), tag);
          return u3m_bail(c3__fail);
        }
        u3_noun frame = _pop_list(&sat_u->susp_list);
        if (u3_none != frame && lst_try != u3h(frame))
        {
          printf(ERR("wrong frame: try"));
          return u3m_bail(c3__fail);
        }
        u3z(frame);
      }

      if (0 == u3h(yil))
      {
        //  any unconstrained nock computation is a potential urwasm reentry:
        //  save the pointers before that, restore after
        uw_arena* box_arena_frame = BoxArena;
        uw_arena* code_arena_frame = CodeArena;
        monad_cont = uw_slam_check(
          u3k(cont),
          u3k(u3t(yil)),
          sat_u->is_stateful
        );
        BoxArena = box_arena_frame;
        CodeArena = code_arena_frame;
        u3z(yil);
        yil = u3_none;
      }
    }

    u3z(monad);
    if (u3_none == yil)
    {
      return _reduce_monad(monad_cont, sat_u);
    }
    else
    {
      return yil;
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->catch_bat))
  {
    //  catch
    u3_noun monad_try = u3at(120, monad);
    u3_noun monad_catch = u3at(121, monad);
    u3_noun cont = u3at(61, monad);
    u3_weak yil;
    u3_noun monad_cont;

    {
      { // push on suspend stacks
        m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_catch_try);
        m3_SuspendStackPushExtTag(sat_u->wasm_module->runtime);
        _push_list(
          u3nt(lst_catch_try, u3k(monad_catch), u3k(cont)),
          &sat_u->susp_list
        );
      }
      yil = _reduce_monad(u3k(monad_try), sat_u);

      if (1 != u3h(yil))
      { // pop suspend stacks
        m3_SuspendStackPopExtTag(sat_u->wasm_module->runtime);
        c3_d tag;
        m3_SuspendStackPop64(sat_u->wasm_module->runtime, &tag);
        if (tag != -1 && tag != west_catch_try)
        {
          printf(ERR("catch-try tag mismatch: %"PRIc3_d), tag);
          return u3m_bail(c3__fail);
        }
        u3_noun frame = _pop_list(&sat_u->susp_list);
        if (u3_none != frame && lst_catch_try != u3h(frame))
        {
          printf(ERR("wrong frame: catch-try"));
          return u3m_bail(c3__fail);
        }
        u3z(frame);
      }

      if (0 == u3h(yil))
      {
        uw_arena* box_arena_frame = BoxArena;
        uw_arena* code_arena_frame = CodeArena;
        monad_cont = uw_slam_check(
          u3k(cont),
          u3k(u3t(yil)),
          sat_u->is_stateful
        );
        BoxArena = box_arena_frame;
        CodeArena = code_arena_frame;
        u3z(yil);
        yil = u3_none;
      }
      else if (2 == u3h(yil))
      {
        u3z(yil);

        { // push on suspend stacks
          m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_catch_err);
          m3_SuspendStackPushExtTag(sat_u->wasm_module->runtime);
          _push_list(
            u3nc(lst_catch_err, u3k(cont)),
            &sat_u->susp_list
          );
        }

        yil = _reduce_monad(u3k(monad_catch), sat_u);

        if (1 != u3h(yil))
        { // pop suspend stacks
          m3_SuspendStackPopExtTag(sat_u->wasm_module->runtime);
          c3_d tag;
          m3_SuspendStackPop64(sat_u->wasm_module->runtime, &tag);
          if (tag != -1 && tag != west_catch_err)
          {
            printf(ERR("catch-err tag mismatch: %"PRIc3_d), tag);
            return u3m_bail(c3__fail);
          }
          u3_noun frame = _pop_list(&sat_u->susp_list);
          if (u3_none != frame && lst_catch_err != u3h(frame))
          {
            printf(ERR("wrong frame: catch-err"));
            return u3m_bail(c3__fail);
          }
          u3z(frame);
        }

        if (0 == u3h(yil))
        {
          uw_arena* box_arena_frame = BoxArena;
          uw_arena* code_arena_frame = CodeArena;
          monad_cont = uw_slam_check(
            u3k(cont),
            u3k(u3t(yil)),
            sat_u->is_stateful
          );
          BoxArena = box_arena_frame;
          CodeArena = code_arena_frame;
          u3z(yil);
          yil = u3_none;
        }
      }
    }

    u3z(monad);
    if (u3_none == yil)
    {
      return _reduce_monad(monad_cont, sat_u);
    }
    else
    {
      return yil;
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->return_bat))
  {
    //  return
    u3_noun yil = u3nc(0, u3k(u3at(30, monad)));
    u3z(monad);
    return yil;
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->global_set_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->global_set_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  global-set
    u3_atom name = u3x_atom(u3at(arr_sam_2, monad));
    u3_atom value = u3x_atom(u3at(arr_sam_3, monad));

    c3_w met_w = u3r_met(3, name);
    c3_c* name_c = u3a_malloc(met_w + 1);
    u3r_bytes(0, met_w, (c3_y*)name_c, name);
    name_c[met_w] = 0;
    
    IM3Global glob = m3_FindGlobal(sat_u->wasm_module, name_c);

    if (!glob)
    {
      fprintf(stderr, ERR("global %s not found"), name_c);
      return u3m_bail(c3__fail);
    }

    if (!glob->isMutable)
    {
      fprintf(stderr, ERR("global %s not mutable"), name_c);
      return u3m_bail(c3__fail);
    }

    M3TaggedValue glob_value;
    M3Result result = m3_GetGlobal(glob, &glob_value);
    if (result)
    {
      fprintf(stderr, ERR("couldn't get global %s: %s"), name_c, result);
      return u3m_bail(c3__fail);
    }
    switch (glob_value.type)
    {
      default:
      {
        return u3m_bail(c3__fail);
      }
      case c_m3Type_i32:
      {
        glob_value.value.i32 = u3r_word(0, value);
        break;
      }
      case c_m3Type_i64:
      {
        glob_value.value.i64 = u3r_chub(0, value);
        break;
      }
      case c_m3Type_f32:
      {
        glob_value.value.f32 = u3r_word(0, value);
        break;
      }
      case c_m3Type_f64:
      {
        glob_value.value.f64 = u3r_chub(0, value);
        break;
      }
    }
    result = m3_SetGlobal(glob, &glob_value);
    if (result)
    {
      fprintf(stderr, ERR("couldn't set global %s: %s"), name_c, result);
      return u3m_bail(c3__fail);
    }
    u3z(monad);
    u3a_free(name_c);
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->global_get_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->global_get_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  global-get
    u3_atom name = u3x_atom(u3at(arr_sam, monad));

    c3_w met_w = u3r_met(3, name);
    c3_c* name_c = u3a_malloc(met_w + 1);
    u3r_bytes(0, met_w, (c3_y*)name_c, name);
    name_c[met_w] = 0;

    IM3Global glob = m3_FindGlobal(sat_u->wasm_module, name_c);
    if (!glob)
    {
      fprintf(stderr, ERR("global %s not found"), name_c);
      return u3m_bail(c3__fail);
    }

    M3TaggedValue glob_value;
    M3Result result = m3_GetGlobal(glob, &glob_value);
    if (result)
    {
      fprintf(stderr, ERR("couldn't get global %s: %s"), name_c, result);
      return u3m_bail(c3__fail);
    }

    u3_noun out;
    switch (glob_value.type)
    {
      default:
      {
        return u3m_bail(c3__fail);
      }
      case c_m3Type_i32:
      {
        out = u3i_word(glob_value.value.i32);
        break;
      }
      case c_m3Type_i64:
      {
        out = u3i_chub(glob_value.value.i64);
        break;
      }
      case c_m3Type_f32:
      {
        out = u3i_word(glob_value.value.f32);
        break;
      }
      case c_m3Type_f64:
      {
        out = u3i_chub(glob_value.value.f64);
        break;
      }
    }

    u3z(monad);
    u3a_free(name_c);
    return u3nc(0, out);

  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->mem_size_bat))
  {
    if (c3n == u3r_sing(u3at(MONAD_CTX, monad), sat_u->match->mem_size_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memory-size
    if (!sat_u->wasm_module->memoryInfo.hasMemory)
    {
      fprintf(stderr, ERR("memsize no memory"));
      return u3m_bail(c3__fail);
    }
    c3_w num_pages = sat_u->wasm_module->runtime->memory.numPages;

    u3z(monad);
    return u3nc(0, u3i_word(num_pages));
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->mem_grow_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->mem_grow_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memory-grow
    if (!sat_u->wasm_module->memoryInfo.hasMemory)
    {
      fprintf(stderr, ERR("memgrow no memory"));
      return u3m_bail(c3__fail);
    }

    u3_noun delta = u3at(arr_sam, monad);

    c3_l delta_l = (c3y == u3a_is_cat(delta)) ? delta : u3m_bail(c3__fail);

    c3_w n_pages = sat_u->wasm_module->runtime->memory.numPages;
    c3_w required_pages = n_pages + delta_l;

    M3Result result = ResizeMemory(sat_u->wasm_module->runtime, required_pages);

    if (result)
    {
      fprintf(stderr, ERR("failed to resize memory: %s"), result);
      return u3m_bail(c3__fail);
    }

    u3z(monad);
    return u3nc(0, u3i_word(n_pages));
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->get_acc_bat))
  {
    u3z(monad);
    return u3nc(0, u3k(sat_u->acc));
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->set_acc_bat))
  {
    u3_noun new = u3k(u3at(arr_sam, monad));
    u3z(monad);
    u3z(sat_u->acc);
    sat_u->acc = new;
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->get_all_glob_bat))
  {
    if (c3n == u3r_sing(u3at(MONAD_CTX, monad), sat_u->match->get_all_glob_ctx))
    {
      return u3m_bail(c3__fail);
    }
    u3z(monad);
    u3_noun atoms = u3_nul;
    c3_w n_globals = sat_u->wasm_module->numGlobals;
    c3_w n_globals_import = sat_u->wasm_module->numGlobImports;
    while (n_globals-- > n_globals_import)
    {
      M3Global glob = sat_u->wasm_module->globals[n_globals];
      switch (glob.type)
      {
        default:
        {
          return u3m_bail(c3__fail);
        }
        case c_m3Type_i32:
        {
          atoms = u3nc(u3i_word(glob.intValue), atoms);
          break;
        }
        case c_m3Type_i64:
        {
          atoms = u3nc(u3i_chub(glob.intValue), atoms);
          break;
        }
        case c_m3Type_f32:
        {
          atoms = u3nc(u3i_word(glob.f32Value), atoms);
          break;
        }
        case c_m3Type_f64:
        {
          atoms = u3nc(u3i_chub(glob.f64Value), atoms);
          break;
        }
      }
    }
    return u3nc(0, atoms);
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->set_all_glob_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat_u->match->set_all_glob_ctx))
    {
      return u3m_bail(c3__fail);
    }
    u3_noun atoms = u3at(arr_sam, monad);
    c3_w n_globals = sat_u->wasm_module->numGlobals;
    c3_w n_globals_import = sat_u->wasm_module->numGlobImports;
    for (c3_w i = n_globals_import; i < n_globals; i++)
    {
      IM3Global glob = &sat_u->wasm_module->globals[i];
      u3_noun atom;
      u3x_cell(atoms, &atom, &atoms);
      u3x_atom(atom);
      switch (glob->type)
      {
        default:
        {
          return u3m_bail(c3__fail);
        }
        case c_m3Type_i32:
        {
          glob->intValue = u3r_word(0, atom);
          break;
        }
        case c_m3Type_i64:
        {
          glob->intValue = u3r_chub(0, atom);
          break;
        }
        case c_m3Type_f32:
        {
          glob->f32Value = u3r_word(0, atom);
          break;
        }
        case c_m3Type_f64:
        {
          glob->f64Value = u3r_chub(0, atom);
          break;
        }
      }
    }
    if (u3_nul != atoms)
    {
      fprintf(stderr, WUT("glob list too long"));
      return u3m_bail(c3__exit);
    }
    u3z(monad);
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat_u->match->fail_bat))
  {
    u3z(monad);
    return u3nc(2, 0);
  }
  else
  {
    return u3m_bail(c3__fail);
  }
}

static const M3Result
_resume_callback(M3Result result_m3, IM3Runtime runtime)
{
  if (result_m3 == m3Err_ComputationBlock
    || result_m3 == m3Err_SuspensionError)
  {
    return result_m3;
  }
  M3Result result = m3Err_none;
  lia_state* sat_u = runtime->userdata_resume;
  m3_SuspendStackPopExtTag(runtime);
  c3_d tag_d;
  m3_SuspendStackPop64(runtime, &tag_d);
  switch (tag_d)
  {
    default:
    {
      u3m_bail(c3__fail);
    }
    case west_call:
    {
      c3_d f_idx_d;
      m3_SuspendStackPop64(sat_u->wasm_module->runtime, &f_idx_d);
      u3_noun frame = _pop_list(&sat_u->susp_list);
      if (lst_call != u3h(frame))
      {
        printf(ERR("wrong frame: call"));
        u3m_bail(c3__fail);
      }
      u3_noun name = u3t(frame);
      c3_w met_w = u3r_met(3, name);
      c3_c* name_c = u3a_malloc(met_w + 1);
      u3r_bytes(0, met_w, (c3_y*)name_c, name);
      u3z(frame);
      name_c[met_w] = 0;

      u3_noun yil;
      if (_deterministic_trap(result_m3))
      {
        fprintf(stderr, WUT("%s call trapped: %s"), name_c, result_m3);
        yil = u3nc(2, 0);
      }
      else if (result_m3)
      {
        fprintf(stderr, ERR("%s call failed: %s"), name_c, result_m3);
        u3m_bail(c3__fail);
      }
      else
      {
        IM3Function f = runtime->modules->functions + f_idx_d;
        c3_w n_out_w = f->funcType->numRets;
        c3_d *vals_out = u3a_calloc(n_out_w, sizeof(c3_d));
        void **valptrs_out = u3a_calloc(n_out_w, sizeof(void*));
        for (c3_w i = 0; i < n_out_w; i++)
        {
          valptrs_out[i] = &vals_out[i];
        }
        M3Result result_tmp = m3_GetResults(f,
          n_out_w,
          (const void**)valptrs_out
        );
        if (result_tmp)
        {
          fprintf(stderr,
            ERR("function %s failed to get results: %s"), name_c, result_tmp
          );
          u3m_bail(c3__fail);
        }
        yil = u3nc(0,
          _atoms_from_stack(valptrs_out, n_out_w, f->funcType->types)
        );
        u3a_free(valptrs_out);
        u3a_free(vals_out);
      }
      if (u3_none != sat_u->resolution)
      {
        u3m_bail(c3__fail);
      }
      sat_u->resolution = yil;
      u3a_free(name_c);
      break;
    }

    case west_call_ext:
    {
      if (1 == u3h(sat_u->resolution))
      {
        // it's a new block, it's not yet resolved
        // restore the frame
        //
        m3_SuspendStackPush64(runtime, tag_d);
        m3_SuspendStackPushExtTag(runtime);
        result = m3Err_ComputationBlock;
      }
      // else the block is resolved and sat_u->resolution holds the result
      //
      break;
    }

    case west_try:
    {
      if (1 != u3h(sat_u->resolution))
      {
        u3_noun frame = _pop_list(&sat_u->susp_list);
        if (lst_try != u3h(frame))
        {
          printf(ERR("wrong frame: try"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat_u->resolution))
        {
          u3_noun cont = u3t(frame);
          u3_noun p_res = u3t(sat_u->resolution);
          uw_arena* box_arena_frame = BoxArena;
          uw_arena* code_arena_frame = CodeArena;
          u3_noun monad_cont = uw_slam_check(
            u3k(cont),
            u3k(p_res),
            sat_u->is_stateful
          );
          BoxArena = box_arena_frame;
          CodeArena = code_arena_frame;
          u3z(sat_u->resolution);
          sat_u->resolution = _reduce_monad(monad_cont, sat_u);
        }
        // if %2 then nothing to do, sat_u->resolution already holds %2 result
        //
        u3z(frame);
      }
      else
      {
        // we shouldn't be here
        //
        u3m_bail(c3__fail);
      }
      break;
    }

    case west_catch_try:
    {
      if (1 != u3h(sat_u->resolution))
      {
        u3_noun frame = _pop_list(&sat_u->susp_list);
        if (lst_catch_try != u3h(frame))
        {
          printf(ERR("wrong frame: catch-try"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat_u->resolution))
        {
          u3_noun cont = u3t(u3t(frame));
          u3_noun p_res = u3t(sat_u->resolution);
          uw_arena* box_arena_frame = BoxArena;
          uw_arena* code_arena_frame = CodeArena;
          u3_noun monad_cont = uw_slam_check(
            u3k(cont),
            u3k(p_res),
            sat_u->is_stateful
          );
          BoxArena = box_arena_frame;
          CodeArena = code_arena_frame;
          u3z(sat_u->resolution);
          sat_u->resolution = _reduce_monad(monad_cont, sat_u);
        }
        // %2
        //
        else
        {
          u3_noun cont = u3t(u3t(frame));
          u3_noun monad_catch = u3h(u3t(frame));
          { // push on suspend stacks
            m3_SuspendStackPush64(sat_u->wasm_module->runtime, west_catch_err);
            m3_SuspendStackPushExtTag(runtime);
            _push_list(
              u3nc(lst_catch_err, u3k(cont)),
              &sat_u->susp_list
            );
          }

          u3_noun yil = _reduce_monad(u3k(monad_catch), sat_u);

          if (1 != u3h(yil))
          { // pop suspend stacks
            m3_SuspendStackPopExtTag(runtime);
            c3_d tag;
            m3_SuspendStackPop64(sat_u->wasm_module->runtime, &tag);
            if (tag != -1 && tag != west_catch_err)
            {
              printf(ERR("catch-err tag mismatch: %"PRIc3_d), tag);
              u3m_bail(c3__fail);
            }
            u3_noun frame1 = _pop_list(&sat_u->susp_list);
            if (lst_catch_err != u3h(frame1))
            {
              printf(ERR("wrong frame: catch-err"));
              u3m_bail(c3__fail);
            }
            u3z(frame1);
          }

          if (2 == u3h(yil))
          {
            // sat_u->resolution already has %2, do nothing
            u3z(yil);
          }
          else if (1 == u3h(yil))
          {
            u3z(sat_u->resolution);
            sat_u->resolution = yil;
          }
          else  // %0
          {
            u3_noun p_res = u3t(yil);
            uw_arena* box_arena_frame = BoxArena;
            uw_arena* code_arena_frame = CodeArena;
            u3_noun monad_cont = uw_slam_check(
              u3k(cont),
              u3k(p_res),
              sat_u->is_stateful
            );
            BoxArena = box_arena_frame;
            CodeArena = code_arena_frame;
            u3z(sat_u->resolution);
            u3z(yil);
            sat_u->resolution = _reduce_monad(monad_cont, sat_u);
          }
        }
        u3z(frame);
      }
      else
      {
        // we shouldn't be here
        //
        u3m_bail(c3__fail);
      }
      break;
    }
    case west_catch_err:
    {
      if (1 != u3h(sat_u->resolution))
      {
        u3_noun frame = _pop_list(&sat_u->susp_list);
        if (lst_catch_err != u3h(frame))
        {
          printf(ERR("wrong frame: catch-err"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat_u->resolution))
        {
          u3_noun cont = u3t(frame);
          u3_noun p_res = u3t(sat_u->resolution);
          uw_arena* box_arena_frame = BoxArena;
          uw_arena* code_arena_frame = CodeArena;
          u3_noun monad_cont = uw_slam_check(
            u3k(cont),
            u3k(p_res),
            sat_u->is_stateful
          );
          BoxArena = box_arena_frame;
          CodeArena = code_arena_frame;
          u3z(sat_u->resolution);
          sat_u->resolution = _reduce_monad(monad_cont, sat_u);
        }
        // if %2 then nothing to do, sat_u->resolution already holds %2 result
        //
        u3z(frame);
      }
      else
      {
        // we shouldn't be here
        //
        u3m_bail(c3__fail);
      }
      break;
    }
    case west_link_wasm:
    {
      if (1 != u3h(sat_u->resolution))
      {
        c3_d _sp_offset_d, func_idx_d;
        m3_SuspendStackPop64(runtime, &func_idx_d);
        m3_SuspendStackPop64(runtime, &_sp_offset_d);
        if (2 == u3h(sat_u->resolution))
        {
          u3z(sat_u->resolution);
          sat_u->resolution = u3_none;
          result = UrwasmArrowExit;
        }
        else  // %0
        {
          IM3Function f = runtime->modules->functions + func_idx_d;
          uint64_t * _sp = (uint64_t *)(runtime->base + _sp_offset_d);
          c3_w n_out = f->funcType->numRets;
          c3_y* types = f->funcType->types;
          void **valptrs_out = u3a_calloc(n_out, sizeof(void*));
          const char *mod = f->import.moduleUtf8;
          const char *name = f->import.fieldUtf8;
          for (c3_w i = 0; i < n_out; i++)
          {
            valptrs_out[i] = &_sp[i];
          }
          c3_o pushed = _coins_to_stack(
            u3t(sat_u->resolution),
            valptrs_out,
            n_out,
            types
          );

          if (c3n == pushed)
          {
            printf(ERR("import result type mismatch: %s/%s"), mod, name);
            result = "import result type mismatch";
          }

          u3z(sat_u->resolution);
          sat_u->resolution = u3_none;
          u3a_free(valptrs_out);
        }
      }
      else
      {
        // we shouldn't be here
        //
        u3m_bail(c3__fail);
      }
      break;
    }
  }
  
  return result;
}

//  TRANSFERS sat->arrow_yil if m3Err_ComputationBlock is thrown
static const void *
_link_wasm_with_arrow_map(
  IM3Runtime runtime,
  IM3ImportContext _ctx,
  uint64_t * _sp,
  void * _mem
)
{
  const char *mod = _ctx->function->import.moduleUtf8;
  const char *name = _ctx->function->import.fieldUtf8;
  lia_state* sat_u = _ctx->userdata;

  u3_noun key = u3nc(u3i_string(mod), u3i_string(name));
  u3_weak arrow = u3kdb_get(u3k(sat_u->map), key);
  if (u3_none == arrow)
  {
    fprintf(stderr, ERR("import not found: %s/%s"), mod, name);
    return m3Err_functionImportMissing;
  }
  c3_w n_in  = _ctx->function->funcType->numArgs;
  c3_w n_out = _ctx->function->funcType->numRets;
  c3_y* types = _ctx->function->funcType->types;
  void **valptrs_in = u3a_calloc(n_in, sizeof(void*));
  for (c3_w i = 0; i < n_in; i++)
  {
    valptrs_in[i] = &_sp[i+n_out];
  }
  void **valptrs_out = u3a_calloc(n_out, sizeof(void*));
  for (c3_w i = 0; i < n_out; i++)
  {
    valptrs_out[i] = &_sp[i];
  }
  
  u3_noun coin_wasm_list = _coins_from_stack(valptrs_in, n_in, (types+n_out));
  
  { // push on suspend stacks
    m3_SuspendStackPush64(runtime, (c3_d)((c3_y*)_sp - (c3_y*)runtime->base));
    c3_d func_idx_d = _ctx->function - runtime->modules->functions;
    m3_SuspendStackPush64(runtime, func_idx_d);
    m3_SuspendStackPush64(runtime, west_link_wasm);
    m3_SuspendStackPushExtTag(runtime);
  }

  uw_arena* box_arena_frame = BoxArena;
  uw_arena* code_arena_frame = CodeArena;
  u3_noun script = uw_slam_check(arrow, coin_wasm_list, sat_u->is_stateful);
  BoxArena = box_arena_frame;
  CodeArena = code_arena_frame;
  u3_noun yil = _reduce_monad(script, sat_u); 

  M3Result result = m3Err_none;

  if (1 != u3h(yil))
  { // pop suspend stacks
    m3_SuspendStackPopExtTag(runtime);
    c3_d tag;
    m3_SuspendStackPop64(runtime, &tag);
    if (tag != -1 && tag != west_link_wasm)
    {
      printf(ERR("west_link tag mismatch: %"PRIc3_d), tag);
      u3m_bail(c3__fail);
    }
    m3_SuspendStackPop64(runtime, NULL);
    m3_SuspendStackPop64(runtime, NULL);

  }

  if (1 == u3h(yil))
  {
    if (sat_u->arrow_yil != u3_none)
    {
      u3z(yil);
      result = "non-empty sat_u->arrow_yil on block";
    }
    else
    {
      sat_u->arrow_yil = yil;
      result = m3Err_ComputationBlock;  // start suspending if not yet suspending
    }
  }
  else if (2 == u3h(yil))
  {
    u3z(yil);
    result = UrwasmArrowExit;
  }
  else
  {
    c3_o pushed = _coins_to_stack(u3t(yil), valptrs_out, n_out, types);
    u3z(yil);
    if (c3n == pushed)
    {
      fprintf(stderr, ERR("import result type mismatch: %s/%s"), mod, name);
      result = "import result type mismatch";
    }
  }
  u3a_free(valptrs_in);
  u3a_free(valptrs_out);
  return result;
}

//  key: [uw_run_m seed]
//  stored nouns:
//    $@  ~                                       ::  tombstone value
//    $:  yield=*                                 ::  +2
//        queue=(list script)                     ::  +6
//        box_arena=[buffer=octs padding=@]       ::  [[+56 +57] +29]
//        memory=[buffer=octs max_stack_offset=@] ::  [[+120 +121] +61]
//        runtime_offset=@                        ::  +62
//        lia_shop=(list)                         ::  +126
//        acc=*                                   ::  +254
//        susp_list=(list)                        ::  +255
//    ==
// arguments RETAINED
// on success allocates sat_u->wasm_module->runtime->memory.mallocated
// and initializes the arenas
static c3_t
_get_state(u3_noun hint, u3_noun seed, lia_state* sat_u)
{
  //  u3_weak get = u3z_find_m(u3z_memo_keep, uw_run_m, seed);
  //  XX order of search matters (sentinel value ~
  //  from previous invocation is closer to the home road)
  //  and u3z_find_m searches from home road down, which is the opposite
  //  of what we want
  //
  u3_noun key = u3z_key(uw_run_m, seed);
  u3_weak get = u3z_find_up(key);
  u3z(key);
  
  if (u3_none == get || u3_nul == get)
  {
    return 0;
  }
  else
  {
    u3_noun yil_previous;
    u3_noun queue;
    u3_noun p_box_buffer, q_box_buffer, pad_box;
    u3_noun p_mem_buffer, q_mem_buffer, stack_offset;
    u3_noun runtime_offset;
    u3_noun lia_shop;
    u3_noun acc;
    u3_noun susp_list;

    if ( c3n == u3r_mean(get,
        2,   &yil_previous,
        6,   &queue,
        56,  &p_box_buffer,
        57,  &q_box_buffer,
        29,  &pad_box,
        120, &p_mem_buffer,
        121, &q_mem_buffer,
        61,  &stack_offset,
        62,  &runtime_offset,
        126, &lia_shop,
        254, &acc,
        255, &susp_list,
        0) 
    )
    {
      return u3m_bail(c3__fail);
    }
    c3_w box_len_w = (c3y == u3a_is_cat(p_box_buffer))
      ? p_box_buffer
      : u3m_bail(c3__fail);
    
    c3_w pad_w = (c3y == u3a_is_cat(pad_box))
      ? pad_box
      : u3m_bail(c3__fail);
    
    c3_w run_off_w = (c3y == u3a_is_cat(runtime_offset))
      ? runtime_offset
      : u3m_bail(c3__fail);
    
    c3_w len_buf_w = (c3y == u3a_is_cat(p_mem_buffer))
      ? p_mem_buffer
      : u3m_bail(c3__fail);

    c3_w stk_off_w = (c3y == u3a_is_cat(stack_offset))
      ? stack_offset
      : u3m_bail(c3__fail);
    
    _uw_arena_init_size(BoxArena, box_len_w);
    u3r_bytes(pad_w, box_len_w, BoxArena->buf_y, q_box_buffer);
    _uw_arena_init(CodeArena);

    M3Result result;
    IM3Runtime wasm3_runtime = (IM3Runtime)(BoxArena->buf_y + run_off_w);
    wasm3_runtime->base = BoxArena->buf_y;
    wasm3_runtime->base_transient = CodeArena->buf_y;
    m3_RewritePointersRuntime(wasm3_runtime, BoxArena->buf_y, 0 /*is_store*/);
    IM3Module wasm3_module = wasm3_runtime->modules;
    c3_w n_imports = wasm3_module->numFuncImports;

    // make sure to not touch BoxArena
    m3_SetAllocators(_calloc_bail, _free_bail, _realloc_bail);
    m3_SetTransientAllocators(_calloc_code, _free_code, _realloc_code);
    m3_SetMemoryAllocators(_calloc_bail, _free_bail, _realloc_bail);

    jmp_buf esc;
    CodeArena->esc_u = &esc;
    c3_i jmp_i;

    while (1)
    {
      wasm3_runtime->base_transient = CodeArena->buf_y;
      
      if (0 == (jmp_i = setjmp(esc)))
      {
        for (c3_w i = 0; i < n_imports; i++)
        {
          M3Function f = wasm3_module->functions[i];
          const char* mod  = f.import.moduleUtf8;
          const char* name = f.import.fieldUtf8;

          result = m3_LinkRawFunctionEx(
            wasm3_module, mod, name,
            NULL, &_link_wasm_with_arrow_map,
            sat_u
          );

          if (result)
          {
            fprintf(stderr, ERR("link error: %s"), result);
            return u3m_bail(c3__fail);
          }
        }

        result = m3_CompileModule(wasm3_module);
        if (result)
        {
          fprintf(stderr, ERR("compilation error: %s"), result);
          return u3m_bail(c3__fail);
        }

        break;
      }
      else
      {
        if (jmp_i == c3__code)
        {
          _uw_arena_grow(CodeArena);
        }
        else
        {
          return u3m_bail(c3__fail);
        }
        continue;
      }
    }

    {
      sat_u->yil_previous = u3k(yil_previous);
      sat_u->queue = u3k(queue);
      sat_u->wasm_module = wasm3_module;
      sat_u->lia_shop = u3k(lia_shop);
      sat_u->acc = u3k(acc);
      // sat_u->map to be filled afterwards
      // sat_u->match same
      // sat_u->resolution same
      sat_u->arrow_yil = u3_none;
      sat_u->susp_list = u3k(susp_list);
      M3MemoryHeader* mem = u3a_malloc(len_buf_w + sizeof(M3MemoryHeader));
      mem->runtime = wasm3_runtime;
      mem->maxStack = BoxArena->buf_y + stk_off_w;
      mem->length = len_buf_w;
      u3r_bytes(0, len_buf_w, (u8*)(mem + 1), q_mem_buffer);
      wasm3_runtime->memory.mallocated = mem;
    }

    u3z(get);

    return 1;
  }
}

//  arguments RETAINED, returned yield transfered.
//  transfers sat_u->yil_previous if it is returned, and replaces
//  the struct value with u3_none
static u3_noun
_apply_diff(u3_noun input_tag, u3_noun p_input, lia_state* sat_u)
{
  m3_SetAllocators(_calloc_bail, _free_bail, _realloc_bail);
  m3_SetTransientAllocators(_calloc_bail, _free_bail, _realloc_bail);
  m3_SetMemoryAllocators(u3a_calloc, u3a_free, u3a_realloc);

  if (input_tag == c3y)
  {
    if (sat_u->wasm_module->runtime->edge_suspend)
    {
      //  appended new script but the computation is still suspended:
      //  add script to queue, return previous yield
      if (sat_u->yil_previous == u3_none)
      {
        return u3m_bail(c3__fail);
      }
      sat_u->queue = u3kb_weld(sat_u->queue, u3nc(u3k(p_input), u3_nul));  // snoc
      u3_noun yil = sat_u->yil_previous;
      sat_u->yil_previous = u3_none;
      return yil;
    }
    else
    {
      return _reduce_monad(u3k(p_input), sat_u);
    }
  }
  else
  {
    if (!sat_u->wasm_module->runtime->edge_suspend)
    {
      //  appended external call resolution but no block to resolve:
      //  snoc result to shop, return previous yield
      if (sat_u->yil_previous == u3_none)
      {
        return u3m_bail(c3__fail);
      }
      sat_u->lia_shop = u3kb_weld(sat_u->lia_shop, u3nc(u3k(p_input), u3_nul));  // snoc
      u3_noun yil = sat_u->yil_previous;
      sat_u->yil_previous = u3_none;
      return yil;
    }
    // else resume
    IM3Runtime run_u = sat_u->wasm_module->runtime;
    run_u->resume_external = _resume_callback;
    run_u->userdata_resume = sat_u;
    if (sat_u->resolution != u3_none)
    {
      return u3m_bail(c3__fail);
    }
    sat_u->resolution = u3nc(0, u3k(p_input));
    M3Result result = m3_Resume(run_u);
    u3_noun yil;
    if (result == m3Err_ComputationBlock)
    {
      yil = sat_u->resolution;
      sat_u->resolution = u3_none;
      if (yil == u3_none)
      {
        yil = sat_u->arrow_yil;
        sat_u->arrow_yil = u3_none;
        if (yil == u3_none)
        {
          return u3m_bail(c3__fail);
        }
      }
    }
    else if (_deterministic_trap(result))
    {
      fprintf(stderr, WUT("function call trapped: %s"), result);  // XX get name of entry function?
      yil = u3nc(2, 0);
    }
    else if (result == m3Err_functionImportMissing)
    {
      return u3m_bail(c3__exit);
    }
    else if (result)
    {
      fprintf(stderr, ERR("resumption failed: %s"), result);
      return u3m_bail(c3__fail);
    }
    else
    {
      yil = sat_u->resolution;
      sat_u->resolution = u3_none;
      if (yil == u3_none)
      {
        return u3m_bail(c3__fail);
      }

      if (sat_u->queue != u3_none)
      {
        while (u3h(yil) == 0 && sat_u->queue != u3_nul)
        {
          u3z(yil);
          u3_noun deferred_script = _pop_list(&sat_u->queue);
          yil = _reduce_monad(deferred_script, sat_u);
        }
      }
    }

    return yil;
  }
}

// try to save new state, replacing old state with a tombstone value
// frees wasm3 memory buffer, releases arenas
// RETAINS arguments, transfers sat_u->lia_shop/susp_list/queue and
// replaces them with u3_none if save is succesful
static void
_move_state(
  lia_state* sat_u,
  u3_noun seed_old,
  u3_noun seed_new,
  u3_noun hint,
  u3_noun yil)
{
  if ( (c3__oust == hint)
      || (2 == u3h(yil))
      || (c3__rand == hint && 0 == u3h(yil))
  )
  {
    u3z_save_m(u3z_memo_keep, uw_run_m, seed_old, u3_nul);
    IM3Runtime run_u = sat_u->wasm_module->runtime;
    M3MemoryHeader* mem_u = run_u->memory.mallocated;
    u3a_free(mem_u);
    _uw_arena_free(CodeArena);
    _uw_arena_free(BoxArena);
    return;
  }

  IM3Runtime run_u = sat_u->wasm_module->runtime;
  M3MemoryHeader* mem_u = run_u->memory.mallocated;
  c3_w stk_off_w = (u8*)mem_u->maxStack - BoxArena->buf_y;
  if (c3n == u3a_is_cat(stk_off_w))
  {
    u3m_bail(c3__fail);
  }

  c3_w len_buf_w = mem_u->length;
  if (c3n == u3a_is_cat(len_buf_w))
  {
    u3m_bail(c3__fail);
  }

  u3_atom q_buf = u3i_bytes(len_buf_w, (c3_y*)(mem_u + 1));

  u3a_free(mem_u);

  m3_RewritePointersRuntime(run_u, BoxArena->buf_y, 1 /*is_store*/);
  c3_w run_off_w = (c3_y*)run_u - BoxArena->buf_y;
  if (c3n == u3a_is_cat(run_off_w))
  {
    u3m_bail(c3__fail);
  }
  
  _uw_arena_free(CodeArena);

  c3_w box_len_w = BoxArena->siz_w;
  if (c3n == u3a_is_cat(box_len_w))
  {
    u3m_bail(c3__fail);
  }

  c3_y pad_y = BoxArena->pad_y;

  u3_atom q_box = u3i_slab_mint(&BoxArena->sab_u);
  BoxArena->ini_t = 0;

  u3_noun stash = uw_octo(
    u3k(yil),
    sat_u->queue,
    u3nc(u3nc(box_len_w, q_box), pad_y),
    u3nc(u3nc(len_buf_w, q_buf), stk_off_w),
    run_off_w,
    sat_u->lia_shop,
    u3k(sat_u->acc), // accumulator will be returned
    sat_u->susp_list
  );
  sat_u->lia_shop = u3_none;
  sat_u->susp_list = u3_none;
  sat_u->queue = u3_none;

  u3z_save_m(u3z_memo_keep, uw_run_m, seed_old, u3_nul);

  u3z_save_m(u3z_memo_keep, uw_run_m, seed_new, stash);

  u3z(stash);
}

u3_weak
u3we_lia_run_v1(u3_noun cor)
{
#ifndef URWASM_STATEFUL
  return u3_none;
#else

  u3_noun hint = u3at(u3x_sam_7, cor);
  if (c3__none == hint)
  {
    return u3_none;
  }

  // strand: save %1, delete in other cases
  c3_t rand_t = (c3__rand == hint);
  
  // agent: always save
  c3_t gent_t = (c3__gent == hint);

  // oust: don't save
  c3_t oust_t = (c3__oust == hint);
  
  //  omit: run statelessly
  c3_t omit_t = !(rand_t || gent_t || oust_t);

  #ifdef URWASM_SUBROAD

  //  enter subroad, 4MB safety buffer
  u3m_hate(1 << 20);

  #endif

  u3_noun ctx = u3at(RUN_CTX, cor);
  u3r_mug(ctx);
    
  u3_noun input = u3at(u3x_sam_2, cor);
  u3_noun seed = u3at(u3x_sam_6, cor);
  
  u3_noun runnable = uw_kick_nock(u3k(ctx), AX_RUNNABLE);
  u3_noun arrows   = KICK1(uw_kick_nock(u3k(ctx), AX_ARROWS));

  u3_noun try_gate = uw_kick_nock(u3k(runnable), AX_TRY);
  u3_noun try_gate_inner = KICK1(try_gate);

  u3_noun seed_new;  
  u3_noun input_tag, p_input;
  u3x_cell(input, &input_tag, &p_input);

  if (input_tag == c3y)
  {
    u3_noun p_input_gate = u3nt(u3nc(0, 7), 0, u3k(p_input));  //  =>(p.input |=(* +>))
    u3_noun past_new = uw_slam_nock(
      u3k(try_gate_inner),
      u3nc(
        u3k(u3at(seed_past, seed)),
        p_input_gate
      )
    );
    seed_new = u3nq(
      u3k(u3at(seed_module, seed)),
      past_new,
      u3k(u3at(seed_shop, seed)),
      u3k(u3at(seed_import, seed))
    );
  }
  else if (input_tag == c3n)
  {
    seed_new = u3nq(
      u3k(u3at(seed_module, seed)),
      u3k(u3at(seed_past, seed)),
      u3nc(u3k(p_input), u3k(u3at(seed_shop, seed))),
      u3k(u3at(seed_import, seed))
    );
  }
  else
  {
    return u3m_bail(c3__fail);
  }

  u3_noun call_script         = KICK1(uw_kick_nock(u3k(arrows), AX_CALL));  
  u3_noun memread_script      = KICK1(uw_kick_nock(u3k(arrows), AX_MEMREAD));  
  u3_noun memwrite_script     = KICK1(uw_kick_nock(u3k(arrows), AX_MEMWRITE));  
  u3_noun call_ext_script     = KICK1(uw_kick_nock(u3k(arrows), AX_CALL_EXT));
  u3_noun global_set_script   = KICK1(uw_kick_nock(u3k(arrows), AX_GLOBAL_SET));
  u3_noun global_get_script   = KICK1(uw_kick_nock(u3k(arrows), AX_GLOBAL_GET));
  u3_noun mem_grow_script     = KICK1(uw_kick_nock(u3k(arrows), AX_MEM_GROW));
  u3_noun mem_size_script     =       uw_kick_nock(u3k(arrows), AX_MEM_SIZE);
  u3_noun get_acc_script      =       uw_kick_nock(u3k(arrows), AX_GET_ACC);
  u3_noun set_acc_script      = KICK1(uw_kick_nock(u3k(arrows), AX_SET_ACC));
  u3_noun get_all_glob_script =       uw_kick_nock(u3k(arrows), AX_GET_ALL_GLOB);
  u3_noun set_all_glob_script = KICK1(uw_kick_nock(    arrows,  AX_SET_ALL_GLOB));

  u3_noun try_script    = KICK1(try_gate_inner);
  u3_noun catch_script  = KICK2(uw_kick_nock(u3k(runnable), AX_CATCH));
  u3_noun return_script = KICK1(uw_kick_nock(u3k(runnable), AX_RETURN));
  u3_noun fail_script =         uw_kick_nock(    runnable,  AX_FAIL);
  
  u3_noun call_bat = u3k(u3h(call_script));
  u3_noun memread_bat = u3k(u3h(memread_script));
  u3_noun memwrite_bat = u3k(u3h(memwrite_script));
  u3_noun call_ext_bat = u3k(u3h(call_ext_script));
  u3_noun try_bat = u3k(u3h(try_script));
  u3_noun catch_bat = u3k(u3h(catch_script));
  u3_noun return_bat = u3k(u3h(return_script));
  u3_noun fail_bat = u3k(u3h(fail_script));
  u3_noun global_set_bat = u3k(u3h(global_set_script));
  u3_noun global_get_bat = u3k(u3h(global_get_script));
  u3_noun mem_grow_bat = u3k(u3h(mem_grow_script));
  u3_noun mem_size_bat = u3k(u3h(mem_size_script));
  u3_noun get_acc_bat = u3k(u3h(get_acc_script));
  u3_noun set_acc_bat = u3k(u3h(set_acc_script));
  u3_noun get_all_glob_bat = u3k(u3h(get_all_glob_script));
  u3_noun set_all_glob_bat = u3k(u3h(set_all_glob_script));

  u3_noun call_ctx = u3k(u3at(ARROW_CTX, call_script));
  u3_noun memread_ctx = u3k(u3at(ARROW_CTX, memread_script));
  u3_noun memwrite_ctx = u3k(u3at(ARROW_CTX, memwrite_script));
  u3_noun global_set_ctx = u3k(u3at(ARROW_CTX, global_set_script));
  u3_noun global_get_ctx = u3k(u3at(ARROW_CTX, global_get_script));
  u3_noun mem_grow_ctx = u3k(u3at(ARROW_CTX, mem_grow_script));
  u3_noun mem_size_ctx = u3k(u3at(MONAD_CTX, mem_size_script));
  u3_noun get_all_glob_ctx = u3k(u3at(MONAD_CTX, get_all_glob_script));
  u3_noun set_all_glob_ctx = u3k(u3at(ARROW_CTX, set_all_glob_script));

  u3z(call_script);
  u3z(memread_script);
  u3z(memwrite_script);
  u3z(call_ext_script);
  u3z(try_script);
  u3z(catch_script);
  u3z(return_script);
  u3z(fail_script);
  u3z(global_set_script);
  u3z(global_get_script);
  u3z(mem_grow_script);
  u3z(mem_size_script);
  u3z(get_acc_script);
  u3z(set_acc_script);
  u3z(get_all_glob_script);
  u3z(set_all_glob_script);
  
  match_data_struct match = {
    call_bat,
    memread_bat,
    memwrite_bat,
    call_ext_bat,
    try_bat,
    catch_bat,
    return_bat,
    fail_bat,
    global_set_bat,
    global_get_bat,
    mem_grow_bat,
    mem_size_bat,
    get_acc_bat,
    set_acc_bat,
    get_all_glob_bat,
    set_all_glob_bat,
  //
    call_ctx,
    memread_ctx,
    memwrite_ctx,
    global_set_ctx,
    global_get_ctx,
    mem_grow_ctx,
    mem_size_ctx,
    get_all_glob_ctx,
    set_all_glob_ctx,
  };

  lia_state sat;

  BoxArena = &sat.box_arena;
  CodeArena = &sat.code_arena;

  u3_noun yil;
  if (!omit_t)
  {
    sat.is_stateful = 1;
    if (_get_state(hint, seed, &sat))
    {
      sat.map = u3t(u3at(seed_import, seed));
      sat.match = &match;
      sat.resolution = u3_none;
      yil = _apply_diff(input_tag, p_input, &sat);
    }
    else
    { // instantiate state with retries
      u3_noun octs = u3at(seed_module, seed_new);
      u3_noun p_octs, q_octs;
      u3x_cell(octs, &p_octs, &q_octs);
      c3_w bin_len_w = (c3y == u3a_is_cat(p_octs)) ? p_octs
                                                   : u3m_bail(c3__fail);
      c3_y* bin_y;
      M3Result result;
      IM3Environment wasm3_env;
      IM3Runtime wasm3_runtime = NULL;
      IM3Module wasm3_module;

      _uw_arena_init(CodeArena);
      _uw_arena_init(BoxArena);

      m3_SetAllocators(_calloc_box, _free_box, _realloc_box);
      m3_SetTransientAllocators(_calloc_code, _free_code, _realloc_code);
      m3_SetMemoryAllocators(u3a_calloc, u3a_free, u3a_realloc);
      jmp_buf esc;
      CodeArena->esc_u = BoxArena->esc_u = &esc;
      c3_i jmp_i;

      while (1)
      {
        if (0 == (jmp_i = setjmp(esc)))
        {
          bin_y = _calloc_box(bin_len_w, 1);
          u3r_bytes(0, bin_len_w, bin_y, u3x_atom(q_octs));

          wasm3_env = m3_NewEnvironment();
          if (!wasm3_env)
          {
            fprintf(stderr, ERR("env is null"));
            return u3m_bail(c3__fail);
          }

          wasm3_runtime = m3_NewRuntime(
            wasm3_env,
            1 << 21,
            NULL,
            1 /* suspend */
          );
          if (!wasm3_runtime)
          {
            fprintf(stderr, ERR("runtime is null"));
            return u3m_bail(c3__fail);
          }

          result = m3_ParseModule(wasm3_env, &wasm3_module, bin_y, bin_len_w);
          if (result)
          {
            fprintf(stderr, ERR("parse binary error: %s"), result);
            return u3m_bail(c3__fail);
          }

          result = m3_LoadModule(wasm3_runtime, wasm3_module);
          if (result)
          {
            fprintf(stderr, ERR("load module error: %s"), result);
            return u3m_bail(c3__fail);
          }

          result = m3_ValidateModule(wasm3_module);
          if (result)
          {
            fprintf(stderr, ERR("validation error: %s"), result); 
            return u3m_bail(c3__fail);
          }

          c3_w n_imports = wasm3_module->numFuncImports;
          u3_noun lia_shop = u3at(seed_shop, seed_new);
          u3_noun import = u3at(seed_import, seed_new);
          
          u3_noun acc, map;
          u3x_cell(import, &acc, &map);
          {
            sat.yil_previous = u3_none;
            sat.queue = u3_nul;
            sat.wasm_module = wasm3_module;
            sat.lia_shop = u3qb_flop(lia_shop);
            sat.acc = u3k(acc);
            sat.map = map;
            sat.match = &match;
            sat.arrow_yil = u3_none;
            sat.susp_list = u3_nul;
            sat.resolution = u3_none;
          }

          for (c3_w i = 0; i < n_imports; i++)
          {
            M3Function f = wasm3_module->functions[i];
            const char* mod  = f.import.moduleUtf8;
            const char* name = f.import.fieldUtf8;

            result = m3_LinkRawFunctionEx(
              wasm3_module, mod, name,
              NULL, &_link_wasm_with_arrow_map,
              &sat
            );

            if (result)
            {
              fprintf(stderr, ERR("link error: %s"), result);
              return u3m_bail(c3__fail);
            }
          }

          result = m3_CompileModule(wasm3_module);
          if (result)
          {
            fprintf(stderr, ERR("compilation error: %s"), result);
            return u3m_bail(c3__fail);
          }

          break;
        }
        else
        {
          // escaped, grow arena and retry
          if (wasm3_runtime)
          {
            u3a_free(wasm3_runtime->memory.mallocated);
          }

          if (jmp_i == c3__box)
          {
            _uw_arena_grow(BoxArena);
            _uw_arena_reset(CodeArena);
          }
          else if (jmp_i == c3__code)
          {
            _uw_arena_grow(CodeArena);
            _uw_arena_reset(BoxArena);
          }
          else
          {
            return u3m_bail(c3__fail);
          }

          continue;
        }
      }

      wasm3_runtime->base = BoxArena->buf_y;
      wasm3_runtime->base_transient = CodeArena->buf_y;
      //  sanity check: struct and code allocators should not be used
      //  when running wasm
      m3_SetAllocators(_calloc_bail, _free_bail, _realloc_bail);
      m3_SetTransientAllocators(_calloc_bail, _free_bail, _realloc_bail);

      result = m3_RunStart(wasm3_module);

      if (result == m3Err_ComputationBlock)
      {
        yil = sat.arrow_yil;
        sat.arrow_yil = u3_none;
        if (yil == u3_none)
        {
          return u3m_bail(c3__fail);
        }
      }
      else if (_deterministic_trap(result))
      {
        fprintf(stderr, WUT("start function call trapped: %s"), result);
        yil = u3nc(2, 0);
      }
      else if (result == m3Err_functionImportMissing)
      {
        return u3m_bail(c3__exit);
      }
      else if (result)
      {
        fprintf(stderr, ERR("start function failed: %s"), result);
        return u3m_bail(c3__fail);
      }
      else
      {
        u3_noun monad = u3at(seed_past, seed_new);
        yil = _reduce_monad(u3k(monad), &sat);
      }
    }

    _move_state(&sat, seed, seed_new, hint, yil);
  }
  else
  {
    sat.is_stateful = 0;
    M3Result result;
    IM3Environment wasm3_env;
    IM3Runtime wasm3_runtime = NULL;
    IM3Module wasm3_module;
    u3_noun p_octs, q_octs;

    u3_noun octs = u3at(seed_module, seed_new);
    u3x_cell(octs, &p_octs, &q_octs);
    c3_w bin_len_w = (c3y == u3a_is_cat(p_octs)) ? p_octs
                                                 : u3m_bail(c3__fail);
    c3_y* bin_y = u3r_bytes_alloc(0, bin_len_w, u3x_atom(q_octs));

    m3_SetAllocators(u3a_calloc, u3a_free, u3a_realloc);
    m3_SetTransientAllocators(u3a_calloc, u3a_free, u3a_realloc);
    m3_SetMemoryAllocators(u3a_calloc, u3a_free, u3a_realloc);

    wasm3_env = m3_NewEnvironment();
    if (!wasm3_env)
    {
      fprintf(stderr, ERR("env is null"));
      return u3m_bail(c3__fail);
    }

    // 2MB stack
    wasm3_runtime = m3_NewRuntime(wasm3_env, 1 << 21, NULL, 0 /* suspend */);
    if (!wasm3_runtime)
    {
      fprintf(stderr, ERR("runtime is null"));
      return u3m_bail(c3__fail);
    }

    //  save the stack to restore it later before calling m3_FreeRuntime
    //  since it is allocated and freed seperately; no need to do it in
    //  stateful code branch since there we will allocate and free
    //  whole arena

    void* stk_u = wasm3_runtime->stack;

    result = m3_ParseModule(wasm3_env, &wasm3_module, bin_y, bin_len_w);
    if (result)
    {
      fprintf(stderr, ERR("parse binary error: %s"), result);
      return u3m_bail(c3__fail);
    }

    result = m3_LoadModule(wasm3_runtime, wasm3_module);
    if (result)
    {
      fprintf(stderr, ERR("load module error: %s"), result);
      return u3m_bail(c3__fail);
    }

    result = m3_ValidateModule(wasm3_module);
    if (result)
    {
      fprintf(stderr, ERR("validation error: %s"), result); 
      return u3m_bail(c3__fail);
    }

    c3_w n_imports = wasm3_module->numFuncImports;
    u3_noun lia_shop = u3at(seed_shop, seed_new);
    u3_noun import = u3at(seed_import, seed_new);

    u3_noun acc, map;
    u3x_cell(import, &acc, &map);
    {
      sat.yil_previous = u3_none;
      sat.queue = u3_none;
      sat.wasm_module = wasm3_module;
      sat.lia_shop = u3qb_flop(lia_shop);
      sat.acc = u3k(acc);
      sat.map = map;
      sat.match = &match;
      sat.arrow_yil = u3_none;
      sat.susp_list = u3_none;
      sat.resolution = u3_none;
    }

    for (c3_w i = 0; i < n_imports; i++)
    {
      M3Function f = wasm3_module->functions[i];
      const char* mod  = f.import.moduleUtf8;
      const char* name = f.import.fieldUtf8;

      result = m3_LinkRawFunctionEx(
        wasm3_module, mod, name,
        NULL, &_link_wasm_with_arrow_map,
        &sat
      );

      if (result)
      {
        fprintf(stderr, ERR("link error: %s"), result);
        return u3m_bail(c3__fail);
      }
    }

    // don't compile module since here we don't care about the ordering
    // of code pages when we don't suspend, the functions will
    // get compiled on call
    //

    result = m3_RunStart(wasm3_module);

    if (result == m3Err_ComputationBlock)
    {
      yil = sat.arrow_yil;
      sat.arrow_yil = u3_none;
      if (yil == u3_none)
      {
        return u3m_bail(c3__fail);
      }
    }
    else if (_deterministic_trap(result))
    {
      fprintf(stderr, WUT("start function call trapped: %s"), result);
      yil = u3nc(2, 0);
    }
    else if (result == m3Err_functionImportMissing)
    {
      return u3m_bail(c3__exit);
    }
    else if (result)
    {
      fprintf(stderr, ERR("start function failed: %s"), result);
      return u3m_bail(c3__fail);
    }
    else
    {
      u3_noun monad = u3at(seed_past, seed_new);
      yil = _reduce_monad(u3k(monad), &sat);
    }

    wasm3_runtime->stack = stk_u;
    m3_FreeRuntime(wasm3_runtime);
    m3_FreeEnvironment(wasm3_env);
    u3a_free(bin_y);
  }

  //  any of these could be u3_none
  //
  {
    u3z(sat.lia_shop);
    u3z(sat.susp_list);
    u3z(sat.yil_previous);
    u3z(sat.queue);
  }

  u3z(match.call_bat);
  u3z(match.memread_bat);
  u3z(match.memwrite_bat);
  u3z(match.call_ext_bat);
  u3z(match.try_bat);
  u3z(match.catch_bat);
  u3z(match.return_bat);
  u3z(match.fail_bat);
  u3z(match.global_set_bat);
  u3z(match.global_get_bat);
  u3z(match.mem_grow_bat);
  u3z(match.mem_size_bat);

  u3z(match.call_ctx);
  u3z(match.memread_ctx);
  u3z(match.memwrite_ctx);
  u3z(global_set_ctx);
  u3z(global_get_ctx);
  u3z(mem_grow_ctx);
  u3z(mem_size_ctx);

  #ifdef URWASM_SUBROAD
  //  exit subroad, copying the result
  u3_noun pro = u3m_love(u3nc(u3nc(yil, sat.acc), seed_new));
  #else
  u3_noun pro = u3nc(u3nc(yil, sat.acc), seed_new);
  #endif

  return pro;

#endif // URWASM_STATEFUL
}


u3_weak
u3we_lia_run_once(u3_noun cor)
{
  if (c3__none == u3at(u3x_sam_6, cor))
  {
    return u3_none;
  }

  #ifdef URWASM_SUBROAD
  //  enter subroad, 4MB safety buffer
  u3m_hate(1 << 20);
  #endif

  u3_noun ctx = u3at(ONCE_CTX, cor);
  u3r_mug(ctx);

  u3_noun runnable = uw_kick_nock(u3k(ctx), AX_RUNNABLE);
  u3_noun arrows   = KICK1(uw_kick_nock(u3k(ctx), AX_ARROWS));

  u3_noun call_script         = KICK1(uw_kick_nock(u3k(arrows), AX_CALL));  
  u3_noun memread_script      = KICK1(uw_kick_nock(u3k(arrows), AX_MEMREAD));  
  u3_noun memwrite_script     = KICK1(uw_kick_nock(u3k(arrows), AX_MEMWRITE));  
  u3_noun call_ext_script     = KICK1(uw_kick_nock(u3k(arrows), AX_CALL_EXT));
  u3_noun global_set_script   = KICK1(uw_kick_nock(u3k(arrows), AX_GLOBAL_SET));
  u3_noun global_get_script   = KICK1(uw_kick_nock(u3k(arrows), AX_GLOBAL_GET));
  u3_noun mem_grow_script     = KICK1(uw_kick_nock(u3k(arrows), AX_MEM_GROW));
  u3_noun mem_size_script     =       uw_kick_nock(u3k(arrows), AX_MEM_SIZE);
  u3_noun get_acc_script      =       uw_kick_nock(u3k(arrows), AX_GET_ACC);
  u3_noun set_acc_script      = KICK1(uw_kick_nock(u3k(arrows), AX_SET_ACC));
  u3_noun get_all_glob_script =       uw_kick_nock(u3k(arrows), AX_GET_ALL_GLOB);
  u3_noun set_all_glob_script = KICK1(uw_kick_nock(    arrows,  AX_SET_ALL_GLOB));

  u3_noun try_script    = KICK2(uw_kick_nock(u3k(runnable), AX_TRY));  
  u3_noun catch_script  = KICK2(uw_kick_nock(u3k(runnable), AX_CATCH));
  u3_noun return_script = KICK1(uw_kick_nock(u3k(runnable), AX_RETURN));
  u3_noun fail_script =         uw_kick_nock(    runnable,  AX_FAIL);
  
  u3_noun call_bat = u3k(u3h(call_script));
  u3_noun memread_bat = u3k(u3h(memread_script));
  u3_noun memwrite_bat = u3k(u3h(memwrite_script));
  u3_noun call_ext_bat = u3k(u3h(call_ext_script));
  u3_noun try_bat = u3k(u3h(try_script));
  u3_noun catch_bat = u3k(u3h(catch_script));
  u3_noun return_bat = u3k(u3h(return_script));
  u3_noun fail_bat = u3k(u3h(fail_script));
  u3_noun global_set_bat = u3k(u3h(global_set_script));
  u3_noun global_get_bat = u3k(u3h(global_get_script));
  u3_noun mem_grow_bat = u3k(u3h(mem_grow_script));
  u3_noun mem_size_bat = u3k(u3h(mem_size_script));
  u3_noun get_acc_bat = u3k(u3h(get_acc_script));
  u3_noun set_acc_bat = u3k(u3h(set_acc_script));
  u3_noun get_all_glob_bat = u3k(u3h(get_all_glob_script));
  u3_noun set_all_glob_bat = u3k(u3h(set_all_glob_script));

  u3_noun call_ctx = u3k(u3at(ARROW_CTX, call_script));
  u3_noun memread_ctx = u3k(u3at(ARROW_CTX, memread_script));
  u3_noun memwrite_ctx = u3k(u3at(ARROW_CTX, memwrite_script));
  u3_noun global_set_ctx = u3k(u3at(ARROW_CTX, global_set_script));
  u3_noun global_get_ctx = u3k(u3at(ARROW_CTX, global_get_script));
  u3_noun mem_grow_ctx = u3k(u3at(ARROW_CTX, mem_grow_script));
  u3_noun mem_size_ctx = u3k(u3at(MONAD_CTX, mem_size_script));
  u3_noun get_all_glob_ctx = u3k(u3at(MONAD_CTX, get_all_glob_script));
  u3_noun set_all_glob_ctx = u3k(u3at(ARROW_CTX, set_all_glob_script));

  u3z(call_script);
  u3z(memread_script);
  u3z(memwrite_script);
  u3z(call_ext_script);
  u3z(try_script);
  u3z(catch_script);
  u3z(return_script);
  u3z(fail_script);
  u3z(global_set_script);
  u3z(global_get_script);
  u3z(mem_grow_script);
  u3z(mem_size_script);
  u3z(get_acc_script);
  u3z(set_acc_script);
  u3z(get_all_glob_script);
  u3z(set_all_glob_script);
  
  
  match_data_struct match = {
    call_bat,
    memread_bat,
    memwrite_bat,
    call_ext_bat,
    try_bat,
    catch_bat,
    return_bat,
    fail_bat,
    global_set_bat,
    global_get_bat,
    mem_grow_bat,
    mem_size_bat,
    get_acc_bat,
    set_acc_bat,
    get_all_glob_bat,
    set_all_glob_bat,
  //
    call_ctx,
    memread_ctx,
    memwrite_ctx,
    global_set_ctx,
    global_get_ctx,
    mem_grow_ctx,
    mem_size_ctx,
    get_all_glob_ctx,
    set_all_glob_ctx,
  };

  u3_noun octs = u3at(u3x_sam_4, cor);
  u3_noun p_octs, q_octs;
  u3x_cell(octs, &p_octs, &q_octs);

  c3_w bin_len_w = (c3y == u3a_is_cat(p_octs)) ? p_octs : u3m_bail(c3__fail);
  c3_y* bin_y = u3r_bytes_alloc(0, bin_len_w, u3x_atom(q_octs));

  M3Result result;

  m3_SetAllocators(u3a_calloc, u3a_free, u3a_realloc);
  m3_SetTransientAllocators(u3a_calloc, u3a_free, u3a_realloc);
  m3_SetMemoryAllocators(u3a_calloc, u3a_free, u3a_realloc);

  IM3Environment wasm3_env = m3_NewEnvironment();
  if (!wasm3_env)
  {
    fprintf(stderr, ERR("env is null"));
    return u3m_bail(c3__fail);
  }
  
  // 2MB stack
  IM3Runtime wasm3_runtime = m3_NewRuntime(
    wasm3_env,
    1 << 21,
    NULL,
    0 /* suspend */
  );
  if (!wasm3_runtime)
  {
    fprintf(stderr, ERR("runtime is null"));
    return u3m_bail(c3__fail);
  }

  void* stk_u = wasm3_runtime->stack;

  IM3Module wasm3_module;
  result = m3_ParseModule(wasm3_env, &wasm3_module, bin_y, bin_len_w);
  if (result)
  {
    fprintf(stderr, ERR("parse binary error: %s"), result);
    return u3m_bail(c3__fail);
  }

  result = m3_LoadModule(wasm3_runtime, wasm3_module);
  if (result)
  {
    fprintf(stderr, ERR("load module error: %s"), result);
    return u3m_bail(c3__fail);
  }

  result = m3_ValidateModule(wasm3_module);
  if (result)
  {
    fprintf(stderr, ERR("validation error: %s"), result); 
    return u3m_bail(c3__fail);
  }

  c3_w n_imports = wasm3_module->numFuncImports;
  u3_noun monad = u3at(u3x_sam_7, cor);
  u3_noun import = u3at(u3x_sam_5, cor);

  u3_noun acc, map;
  u3x_cell(import, &acc, &map);

  lia_state sat = {
    wasm3_module,
    u3_nul,
    u3k(acc),
    map,
    &match,
    u3_none,
    u3_none,
    u3_none
  };

  sat.is_stateful = 0;

  for (c3_w i = 0; i < n_imports; i++)
  {
    M3Function f = wasm3_module->functions[i];
    const char * mod  = f.import.moduleUtf8;
    const char * name = f.import.fieldUtf8;

    result = m3_LinkRawFunctionEx(
      wasm3_module, mod, name,
      NULL, &_link_wasm_with_arrow_map,
      (void *)&sat
    );

    if (result)
    {
      fprintf(stderr, ERR("link error: %s"), result);
      return u3m_bail(c3__fail);
    }
  }

  u3_noun yil;

  result = m3_RunStart(wasm3_module);

  if (result == m3Err_ComputationBlock)
  {
    yil = sat.arrow_yil;
    sat.arrow_yil = u3_none;
    if (yil == u3_none)
    {
      return u3m_bail(c3__fail);
    }
  }
  else if (_deterministic_trap(result))
  {
    fprintf(stderr, WUT("start function call trapped: %s"), result);
    yil = u3nc(2, 0);
  }
  else if (result == m3Err_functionImportMissing)
  {
    return u3m_bail(c3__exit);
  }
  else if (result)
  {
    fprintf(stderr, ERR("start function failed: %s"), result);
    return u3m_bail(c3__fail);
  }
  else
  {
    yil = _reduce_monad(u3k(monad), &sat);
  }

  wasm3_runtime->stack = stk_u;
  m3_FreeRuntime(wasm3_runtime);
  m3_FreeEnvironment(wasm3_env);

  u3a_free(bin_y);

  u3z(match.call_bat);
  u3z(match.memread_bat);
  u3z(match.memwrite_bat);
  u3z(match.call_ext_bat);
  u3z(match.try_bat);
  u3z(match.catch_bat);
  u3z(match.return_bat);
  u3z(match.fail_bat);
  u3z(match.global_set_bat);
  u3z(match.global_get_bat);
  u3z(match.mem_grow_bat);
  u3z(match.mem_size_bat);
  u3z(match.get_acc_bat);
  u3z(match.set_acc_bat);
  u3z(match.get_all_glob_bat);
  u3z(match.set_all_glob_bat);

  u3z(match.call_ctx);
  u3z(match.memread_ctx);
  u3z(match.memwrite_ctx);
  u3z(global_set_ctx);
  u3z(global_get_ctx);
  u3z(mem_grow_ctx);
  u3z(mem_size_ctx);
  u3z(get_all_glob_ctx);
  u3z(set_all_glob_ctx);
  
  #ifdef URWASM_SUBROAD
  //  exit subroad, copying the result
  u3_noun pro = u3m_love(u3nc(yil, sat.acc));
  #else
  u3_noun pro = u3nc(yil, sat.acc);
  #endif

  return pro;
}
