/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"
#include "m3_validate.h"

// #define URWASM_SUBROAD
// #define URWASM_STATEFUL

#define ONCE_CTX        63
#define RUN_CTX         7

#define AX_RUNNABLE     374
#define AX_ARROWS       1502

#define AX_CALL         20
#define AX_MEMREAD      383
#define AX_MEMWRITE     94
#define AX_CALL_EXT     375
#define AX_GLOBAL_SET   4
#define AX_GLOBAL_GET   22
#define AX_MEM_SIZE     186
#define AX_MEM_GROW     381
#define AX_GET_ACC      374
#define AX_SET_ACC      92
#define AX_GET_ALL_GLOB 43
#define AX_SET_ALL_GLOB 380

#define AX_TRY          43
#define AX_CATCH        4
#define AX_RETURN       20

#define ARROW_CTX       511
#define MONAD_CTX       127

#define arr_sam         62
#define arr_sam_2       124
#define arr_sam_3       125
#define arr_sam_6       250
#define arr_sam_7       251

#define ERR(string) ("\r\n\033[31m>>> " string "\033[0m\r\n")
#define WUT(string) ("\r\n\033[33m>>  " string "\033[0m\r\n")

static inline void
_push_list(u3_noun som, u3_noun *lit)
{
  *lit = u3nc(som, *lit);
}

static inline u3_noun
_pop_list(u3_noun *lit)
{
  u3_noun hed, tel;
  if (c3n == u3r_cell(*lit, &hed, &tel))
  {
    return u3m_bail(c3__fail);
  }
  u3k(hed), u3k(tel);
  u3z(*lit);
  *lit = tel;
  return hed;
}

static const M3Result UrwasmArrowExit = "An imported arrow returned %2";

typedef struct {
  u3_noun call_bat;
  u3_noun memread_bat;
  u3_noun memwrite_bat;
  u3_noun call_ext_bat;
  u3_noun try_bat;
  u3_noun catch_bat;
  u3_noun return_bat;
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

typedef struct {
  IM3Module wasm_module;  // p
  u3_noun lia_shop;       // q
  u3_noun acc;            // p.r, transferred
  u3_noun map;            // q.r, retained
  match_data_struct* match;
  u3_noun arrow_yil;
  u3_noun susp_list;
  u3_noun resolution; // resolved %1 block
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
  lst_call = 0,
  lst_try = 1,
  lst_catch_try = 2,
  lst_catch_err = 3,
  lst_link_wasm = 4,
} lia_suspend_tag;

typedef struct {
  c3_w      siz_w;  // size in 4K pages, (1 << 12)
  c3_w      pre_w;  // previous size
  c3_y*     buf_y;  // allocated buffer
  c3_y*     nex_y;  // next allocation
  u3i_slab  sab_u;  // associated slab
  jmp_buf   esc_u;  // escape buffer
} uw_arena;

// Code page allocation: simple bump allocator for non-growing objects,
// i.e. code pages
//
static uw_arena CodeArena;

static void*
_calloc_code(size_t num_i, size_t len_i)
{
  
  void* lag_v = CodeArena.nex_y;
  
  size_t byt_i = num_i * len_i;
  if (byt_i / len_i != num_i)
  {
    u3m_bail(c3__fail);
  }

  c3_y* nex_y = CodeArena.nex_y + byt_i;
  nex_y = c3_align(nex_y, 16, C3_ALGHI);
  
  if (nex_y - CodeArena.buf_y >= (CodeArena.siz_w << 12))
  { // OOM, jump out to increase the arena size and try again
    _longjmp(CodeArena.esc_u, 1);
  }

  CodeArena.nex_y = nex_y;
  return lag_v;
}

static void*
_realloc_code(void* lag_v, size_t len_i)
{
  return _calloc_code(len_i, 1);
}

static void
_free_code(void* lag_v)
{
  // noop
}

// Struct/array allocation: bump allocator of boxes with capacity,
// on realloc: try to stay in the box, else allocate a bigger box
// box: [len_d cap_d data]
//
static uw_arena BoxArena;

static void*
_calloc_box_cap(size_t num_i, size_t len_i, c3_d cap_d)
{
  void* lag_v = BoxArena.nex_y;

  size_t byt_i = num_i * len_i;
  if ((byt_i / len_i != num_i) || (byt_i >= UINT64_MAX - 16))
  {
    u3m_bail(c3__fail);
  }

  c3_d byt_d = byt_i + 16;  // size of box

  byt_d = c3_min(byt_d, cap_d + 16); // allocate at least cap_d + 16 bytes

  c3_y* nex_y = BoxArena.nex_y + byt_d;
  nex_y = c3_align(nex_y, 16, C3_ALGHI);

  if (nex_y - BoxArena.buf_y >= (BoxArena.siz_w << 12))
  { // OOM, jump out to increase the arena size and try again
    _longjmp(BoxArena.esc_u, 1);
  }

  BoxArena.nex_y = nex_y;

  *(c3_d*)lag_v = (c3_d)(byt_i);
  *((c3_d*)lag_v + 1) = (byt_d - 16);

  return (void*)((c3_d*)lag_v + 2);
}

static void*
_calloc_box(size_t num_i, size_t len_i)
{
  return _calloc_box_cap(num_i, len_i, 512);
}

static void*
_realloc_box(void* lag_v, size_t len_i)
{
  if (len_i > UINT64_MAX)
  {
    u3m_bail(c3__fail);
  }
  c3_d len_d = len_i;
  c3_d pre_d = *((c3_d*)lag_v - 2);
  if (len_d <= pre_d)
  {
    *((c3_d*)lag_v - 2) = len_d;
    return lag_v;
  }
  else
  {
    c3_d cap_d = *((c3_d*)lag_v - 1);
    if (cap_d < len_d)
    {
      // while (cap_d < len_d) {cap_d <<= 1;}
      //
      c3_d cap_bits_d = c3_bits_dabl(cap_d);
      c3_d len_bits_d = c3_bits_dabl(len_d);
      if (cap_bits_d < len_bits_d)
      {
        cap_d <<= len_bits_d - cap_bits_d;
      }
      if (cap_d < len_d)
      {
        cap_d <<= 1;
      }
      return _calloc_box_cap(len_i, 1, cap_d);
    }
    else
    {
      *((c3_d*)lag_v - 2) = len_d;
      return lag_v;
    }
  }
}

static void
_free_box(void* lag_v)
{
  // noop
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
_reduce_monad(u3_noun monad, lia_state* sat)
{
  u3_noun monad_bat = u3h(monad);
  if (c3y == u3r_sing(monad_bat, sat->match->call_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->call_ctx))
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
    result = m3_FindFunction(&f, sat->wasm_module->runtime, name_c);

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

    c3_w edge_1 = sat->wasm_module->runtime->edge_suspend;

    // printf("\r\n\r\n invoke %s\r\n\r\n", name_c);

    { // push on suspend stacks
      c3_d f_idx_d = f - sat->wasm_module->functions;
      m3_SuspendStackPush64(sat->wasm_module->runtime, f_idx_d);
      m3_SuspendStackPush64(sat->wasm_module->runtime, west_call);
      m3_SuspendStackPushExtTag(sat->wasm_module->runtime);
      _push_list(
        u3nc(lst_call, u3k(name)),
        &sat->susp_list
      );
    }

    M3Result result_call = m3_Call(f, n_in, (const void**)valptrs_in);
    // printf("\r\n done %s\r\n", name_c);

    if (result_call != m3Err_ComputationBlock
      && result_call != m3Err_SuspensionError)
    { // pop suspend stacks
      m3_SuspendStackPopExtTag(sat->wasm_module->runtime);
      c3_d tag;
      m3_SuspendStackPop64(sat->wasm_module->runtime, &tag);
      if (tag != west_call)
      {
        printf(ERR("call tag mismatch: %"PRIc3_d), tag);
        return u3m_bail(c3__fail);
      }
      m3_SuspendStackPop64(sat->wasm_module->runtime, NULL);
      u3_noun frame = _pop_list(&sat->susp_list);
      if (lst_call != u3h(frame))
      {
        printf(ERR("wrong frame: call"));
        return u3m_bail(c3__fail);
      }
      u3z(frame);
    }

    u3_noun yil;
    if (result_call == m3Err_ComputationBlock)
    {
      yil = sat->arrow_yil;
      sat->arrow_yil = u3_none;
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

    c3_w edge_2 = sat->wasm_module->runtime->edge_suspend;
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
  else if (c3y == u3r_sing(monad_bat, sat->match->memread_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->memread_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memread
    u3_atom ptr = u3x_atom(u3at(arr_sam_2, monad));
    u3_noun len = u3at(arr_sam_3, monad);

    c3_w ptr_w = u3r_word(0, ptr);
    c3_l len_l = (c3y == u3a_is_cat(len)) ? len : u3m_bail(c3__fail);
    c3_w len_buf_w;
    c3_y* buf_y = m3_GetMemory(sat->wasm_module->runtime, &len_buf_w, 0);

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
  else if (c3y == u3r_sing(monad_bat, sat->match->memwrite_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->memwrite_ctx))
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
    c3_y* buf_y = m3_GetMemory(sat->wasm_module->runtime, &len_buf_w, 0);

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
  else if (c3y == u3r_sing(monad_bat, sat->match->call_ext_bat))
  {
    //  call-ext
    u3_noun name = u3at(arr_sam_2, monad);
    u3_noun args = u3at(arr_sam_3, monad);
    if (u3_nul == sat->lia_shop)
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
      // A frame is pushed to trigger the callback
      //
      m3_SuspendStackPush64(sat->wasm_module->runtime, west_call_ext);
      m3_SuspendStackPushExtTag(sat->wasm_module->runtime);
      u3_noun yil = u3nt(1, u3k(name), u3k(args));
      u3z(monad);
      return yil;
    }
    else
    {
      u3_noun lia_buy;
      u3x_cell(sat->lia_shop, &lia_buy, &sat->lia_shop);
      u3z(monad);
      return u3nc(0, u3k(lia_buy));
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->try_bat))
  {
    //  try
    u3_noun monad_b = u3at(60, monad);
    u3_noun cont = u3at(61, monad);
    u3_weak yil;
    u3_noun monad_cont;
    { // push on suspend stacks
      m3_SuspendStackPush64(sat->wasm_module->runtime, west_try);
      m3_SuspendStackPushExtTag(sat->wasm_module->runtime);
      _push_list(u3nc(lst_try, u3k(cont)), &sat->susp_list);
    }
    {
      yil = _reduce_monad(u3k(monad_b), sat);

      if (1 != u3h(yil))
      { // pop suspend stacks
        m3_SuspendStackPopExtTag(sat->wasm_module->runtime);
        c3_d tag;
        m3_SuspendStackPop64(sat->wasm_module->runtime, &tag);
        if (tag != west_try)
        {
          printf(ERR("try tag mismatch: %"PRIc3_d), tag);
          return u3m_bail(c3__fail);
        }
        u3_noun frame = _pop_list(&sat->susp_list);
        if (lst_try != u3h(frame))
        {
          printf(ERR("wrong frame: try"));
          return u3m_bail(c3__fail);
        }
        u3z(frame);
      }

      if (0 == u3h(yil))
      {
        monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil)));
        u3z(yil);
        yil = u3_none;
      }
    }

    u3z(monad);
    if (u3_none == yil)
    {
      return _reduce_monad(monad_cont, sat);
    }
    else
    {
      return yil;
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->catch_bat))
  {
    //  catch
    u3_noun monad_try = u3at(120, monad);
    u3_noun monad_catch = u3at(121, monad);
    u3_noun cont = u3at(61, monad);
    u3_weak yil;
    u3_noun monad_cont;

    {
      { // push on suspend stacks
        m3_SuspendStackPush64(sat->wasm_module->runtime, west_catch_try);
        m3_SuspendStackPushExtTag(sat->wasm_module->runtime);
        _push_list(
          u3nt(lst_catch_try, u3k(monad_catch), u3k(cont)),
          &sat->susp_list
        );
      }
      yil = _reduce_monad(u3k(monad_try), sat);

      if (1 != u3h(yil))
      { // pop suspend stacks
        m3_SuspendStackPopExtTag(sat->wasm_module->runtime);
        c3_d tag;
        m3_SuspendStackPop64(sat->wasm_module->runtime, &tag);
        if (tag != west_catch_try)
        {
          printf(ERR("catch-try tag mismatch: %"PRIc3_d), tag);
          return u3m_bail(c3__fail);
        }
        u3_noun frame = _pop_list(&sat->susp_list);
        if (lst_catch_try != u3h(frame))
        {
          printf(ERR("wrong frame: catch-try"));
          return u3m_bail(c3__fail);
        }
        u3z(frame);
      }

      if (0 == u3h(yil))
      {
        monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil)));
        u3z(yil);
        yil = u3_none;
      }
      else if (2 == u3h(yil))
      {
        u3z(yil);

        { // push on suspend stacks
          m3_SuspendStackPush64(sat->wasm_module->runtime, west_catch_err);
          m3_SuspendStackPushExtTag(sat->wasm_module->runtime);
          _push_list(
            u3nc(lst_catch_err, u3k(cont)),
            &sat->susp_list
          );
        }

        yil = _reduce_monad(u3k(monad_catch), sat);

        if (1 != u3h(yil))
        { // pop suspend stacks
          m3_SuspendStackPopExtTag(sat->wasm_module->runtime);
          c3_d tag;
          m3_SuspendStackPop64(sat->wasm_module->runtime, &tag);
          if (tag != west_catch_err)
          {
            printf(ERR("catch-err tag mismatch: %"PRIc3_d), tag);
            return u3m_bail(c3__fail);
          }
          u3_noun frame = _pop_list(&sat->susp_list);
          if (lst_catch_err != u3h(frame))
          {
            printf(ERR("wrong frame: catch-err"));
            return u3m_bail(c3__fail);
          }
          u3z(frame);
        }

        if (0 == u3h(yil))
        {
          monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil)));
          u3z(yil);
          yil = u3_none;
        }
      }
    }

    u3z(monad);
    if (u3_none == yil)
    {
      return _reduce_monad(monad_cont, sat);
    }
    else
    {
      return yil;
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->return_bat))
  {
    //  return
    u3_noun yil = u3nc(0, u3k(u3at(30, monad)));
    u3z(monad);
    return yil;
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->global_set_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->global_set_ctx))
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
    
    IM3Global glob = m3_FindGlobal(sat->wasm_module, name_c);

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
  else if (c3y == u3r_sing(monad_bat, sat->match->global_get_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->global_get_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  global-get
    u3_atom name = u3x_atom(u3at(arr_sam, monad));

    c3_w met_w = u3r_met(3, name);
    c3_c* name_c = u3a_malloc(met_w + 1);
    u3r_bytes(0, met_w, (c3_y*)name_c, name);
    name_c[met_w] = 0;

    IM3Global glob = m3_FindGlobal(sat->wasm_module, name_c);
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
  else if (c3y == u3r_sing(monad_bat, sat->match->mem_size_bat))
  {
    if (c3n == u3r_sing(u3at(MONAD_CTX, monad), sat->match->mem_size_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memory-size
    if (!sat->wasm_module->memoryInfo.hasMemory)
    {
      fprintf(stderr, ERR("memsize no memory"));
      return u3m_bail(c3__fail);
    }
    c3_w num_pages = sat->wasm_module->runtime->memory.numPages;

    u3z(monad);
    return u3nc(0, u3i_word(num_pages));
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->mem_grow_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->mem_grow_ctx))
    {
      return u3m_bail(c3__fail);
    }
    //  memory-grow
    if (!sat->wasm_module->memoryInfo.hasMemory)
    {
      fprintf(stderr, ERR("memgrow no memory"));
      return u3m_bail(c3__fail);
    }

    u3_noun delta = u3at(arr_sam, monad);

    c3_l delta_l = (c3y == u3a_is_cat(delta)) ? delta : u3m_bail(c3__fail);

    c3_w n_pages = sat->wasm_module->runtime->memory.numPages;
    c3_w required_pages = n_pages + delta_l;

    M3Result result = ResizeMemory(sat->wasm_module->runtime, required_pages);

    if (result)
    {
      fprintf(stderr, ERR("failed to resize memory: %s"), result);
      return u3m_bail(c3__fail);
    }

    u3z(monad);
    return u3nc(0, u3i_word(n_pages));
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->get_acc_bat))
  {
    u3z(monad);
    return u3nc(0, u3k(sat->acc));
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->set_acc_bat))
  {
    u3_noun new = u3k(u3at(arr_sam, monad));
    u3z(monad);
    u3z(sat->acc);
    sat->acc = new;
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->get_all_glob_bat))
  {
    if (c3n == u3r_sing(u3at(MONAD_CTX, monad), sat->match->get_all_glob_ctx))
    {
      return u3m_bail(c3__fail);
    }
    u3z(monad);
    u3_noun atoms = u3_nul;
    c3_w n_globals = sat->wasm_module->numGlobals;
    c3_w n_globals_import = sat->wasm_module->numGlobImports;
    while (n_globals-- > n_globals_import)
    {
      M3Global glob = sat->wasm_module->globals[n_globals];
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
  else if (c3y == u3r_sing(monad_bat, sat->match->set_all_glob_bat))
  {
    if (c3n == u3r_sing(u3at(ARROW_CTX, monad), sat->match->set_all_glob_ctx))
    {
      return u3m_bail(c3__fail);
    }
    u3_noun atoms = u3at(arr_sam, monad);
    c3_w n_globals = sat->wasm_module->numGlobals;
    c3_w n_globals_import = sat->wasm_module->numGlobImports;
    for (c3_w i = n_globals_import; i < n_globals; i++)
    {
      IM3Global glob = &sat->wasm_module->globals[i];
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
  lia_state* sat = runtime->userdata_resume;
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
      m3_SuspendStackPop64(sat->wasm_module->runtime, &f_idx_d);
      u3_noun frame = _pop_list(&sat->susp_list);
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
          fprintf(stderr, ERR("function %s failed to get results"), name_c);
          u3m_bail(c3__fail);
        }
        yil = u3nc(0,
          _atoms_from_stack(valptrs_out, n_out_w, f->funcType->types)
        );
        u3a_free(valptrs_out);
        u3a_free(vals_out);
      }
      if (u3_none != sat->resolution)
      {
        u3m_bail(c3__fail);
      }
      sat->resolution = yil;
      u3a_free(name_c);
      break;
    }

    case west_call_ext:
    {
      if (1 == u3h(sat->resolution))
      {
        // it's a new block, it's not yet resolved
        // restore the frame
        //
        m3_SuspendStackPush64(runtime, tag_d);
        m3_SuspendStackPopExtTag(runtime);
        result = m3Err_ComputationBlock;
      }
      // else the block is resolved and sat->resolution holds the result
      //
      break;
    }

    case west_try:
    {
      if (1 != u3h(sat->resolution))
      {
        u3_noun frame = _pop_list(&sat->susp_list);
        if (lst_try != u3h(frame))
        {
          printf(ERR("wrong frame: try"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat->resolution))
        {
          u3_noun cont = u3t(frame);
          u3_noun p_res = u3t(sat->resolution);
          u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(p_res));
          u3z(sat->resolution);
          sat->resolution = _reduce_monad(monad_cont, sat);
        }
        // if %2 then nothing to do, sat->resolution already holds %2 result
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
      if (1 != u3h(sat->resolution))
      {
        u3_noun frame = _pop_list(&sat->susp_list);
        if (lst_catch_try != u3h(frame))
        {
          printf(ERR("wrong frame: catch-try"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat->resolution))
        {
          u3_noun cont = u3t(u3t(frame));
          u3_noun p_res = u3t(sat->resolution);
          u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(p_res));
          u3z(sat->resolution);
          sat->resolution = _reduce_monad(monad_cont, sat);
        }
        // %2
        //
        else
        {
          u3_noun cont = u3t(u3t(frame));
          u3_noun monad_catch = u3h(u3t(frame));
          { // push on suspend stacks
            m3_SuspendStackPush64(sat->wasm_module->runtime, west_catch_err);
            m3_SuspendStackPushExtTag(runtime);
            _push_list(
              u3nc(lst_catch_err, u3k(cont)),
              &sat->susp_list
            );
          }

          u3_noun yil = _reduce_monad(u3k(monad_catch), sat);

          if (1 != u3h(yil))
          { // pop suspend stacks
            m3_SuspendStackPopExtTag(runtime);
            c3_d tag;
            m3_SuspendStackPop64(sat->wasm_module->runtime, &tag);
            if (tag != west_catch_err)
            {
              printf(ERR("catch-err tag mismatch: %"PRIc3_d), tag);
              u3m_bail(c3__fail);
            }
            u3_noun frame1 = _pop_list(&sat->susp_list);
            if (lst_catch_err != u3h(frame1))
            {
              printf(ERR("wrong frame: catch-err"));
              u3m_bail(c3__fail);
            }
            u3z(frame1);
          }

          if (2 == u3h(yil))
          {
            // sat->resolution already has %2, do nothing
            u3z(yil);
          }
          else if (1 == u3h(yil))
          {
            u3z(sat->resolution);
            sat->resolution = yil;
          }
          else  // %0
          {
            u3_noun p_res = u3t(yil);
            u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(p_res));
            u3z(sat->resolution);
            u3z(yil);
            sat->resolution = _reduce_monad(monad_cont, sat);
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
      if (1 != u3h(sat->resolution))
      {
        u3_noun frame = _pop_list(&sat->susp_list);
        if (lst_catch_err != u3h(frame))
        {
          printf(ERR("wrong frame: catch-err"));
          u3m_bail(c3__fail);
        }
        if (0 == u3h(sat->resolution))
        {
          u3_noun cont = u3t(frame);
          u3_noun p_res = u3t(sat->resolution);
          u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(p_res));
          u3z(sat->resolution);
          sat->resolution = _reduce_monad(monad_cont, sat);
        }
        // if %2 then nothing to do, sat->resolution already holds %2 result
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
      if (1 != u3h(sat->resolution))
      {
        c3_d _sp_offset_d, func_idx_d;
        m3_SuspendStackPop64(runtime, &func_idx_d);
        m3_SuspendStackPop64(runtime, &_sp_offset_d);
        if (2 == u3h(sat->resolution))
        {
          u3z(sat->resolution);
          sat->resolution = u3_none;
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
            u3t(sat->resolution),
            valptrs_out,
            n_out,
            types
          );

          if (c3n == pushed)
          {
            printf(ERR("import result type mismatch: %s/%s"), mod, name);
            result = "import result type mismatch";
          }

          u3z(sat->resolution);
          sat->resolution = u3_none;
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
  lia_state* sat = _ctx->userdata;

  u3_noun key = u3nc(u3i_string(mod), u3i_string(name));
  u3_weak arrow = u3kdb_get(u3k(sat->map), key);
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

  u3_noun script = u3n_slam_on(arrow, coin_wasm_list);
  u3_noun yil = _reduce_monad(script, sat); 

  M3Result result = m3Err_none;

  if (1 != u3h(yil))
  { // pop suspend stacks
    m3_SuspendStackPopExtTag(runtime);
    c3_d tag;
    m3_SuspendStackPop64(runtime, &tag);
    if (tag != west_link_wasm)
    {
      printf(ERR("west_link tag mismatch: %"PRIc3_d), tag);
      u3m_bail(c3__fail);
    }
    m3_SuspendStackPop64(runtime, NULL);
    m3_SuspendStackPop64(runtime, NULL);

  }

  if (1 == u3h(yil))
  {
    if (sat->arrow_yil != u3_none)
    {
      u3z(yil);
      result = "non-empty sat->arrow_yil on block";
    }
    else
    {
      sat->arrow_yil = yil;
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

u3_weak
u3we_lia_run(u3_noun cor)
{
#ifndef URWASM_STATEFUL
  return u3_none;
}
#else
  if (c3__none == u3at(u3x_sam_7, cor))
  {
    return u3_none;
  }

  #ifdef URWASM_SUBROAD

  //  enter subroad, 4MB safety buffer
  u3m_hate(1 << 20);

  #endif

  u3r_mug(cor);
    
  u3_noun input = u3at(u3x_sam_2, cor);
  u3_noun seed = u3at(u3x_sam_6, cor);
  
  u3_noun runnable = u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_RUNNABLE);
  u3_noun try_gate = u3j_kink(u3k(runnable), AX_TRY);
  u3_noun try_gate_inner = u3j_kink(try_gate, 2);

  u3_noun seed_new;  
  u3_noun input_tag, p_input;
  u3x_cell(input, &input_tag, &p_input);

  if (input_tag == c3y)
  {
    u3_noun p_input_gate = u3nt(u3nc(0, 7), 0, u3k(p_input));  //  =>(p.input |=(* +>))
    u3_noun past_new = u3n_slam_on(
      u3k(try_gate_inner),
      u3nc(
        u3k(u3at(6, seed)),
        p_input_gate
      )
    );
    seed_new = u3nq(
      u3k(u3at(2, seed)),
      past_new,
      u3k(u3at(14, seed)),
      u3k(u3at(15, seed))
    );
  }
  else if (input_tag == c3n)
  {
    seed_new = u3nq(
      u3k(u3at(2, seed)),
      u3k(u3at(6, seed)),
      u3kb_weld(
        u3k(u3at(14, seed)),
        u3nc(u3k(p_input), u3_nul)
      ),
      u3k(u3at(15, seed))
    );
  }
  else
  {
    return u3m_bail(c3__fail);
  }

  u3_noun call_script       = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_CALL), 2);  
  u3_noun memread_script    = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_MEMREAD), 2);  
  u3_noun memwrite_script   = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_MEMWRITE), 2);  
  u3_noun call_ext_script   = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_CALL_EXT), 2);
  u3_noun global_set_script = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_GLOBAL_SET), 2);
  u3_noun global_get_script = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_GLOBAL_GET), 2);
  u3_noun mem_grow_script   = u3j_kink(u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_MEM_GROW), 2);
  u3_noun mem_size_script   =          u3j_kink(u3k(u3at(RUN_CTX, cor)), AX_MEM_SIZE);

  u3_noun try_script = u3j_kink(try_gate_inner, 2);
  u3_noun catch_script  = u3j_kink(u3j_kink(u3j_kink(u3k(runnable), AX_CATCH), 2), 2);  
  u3_noun return_script =          u3j_kink(u3j_kink(runnable,      AX_RETURN), 2);  
  
  u3_noun call_bat = u3k(u3h(call_script));
  u3_noun memread_bat = u3k(u3h(memread_script));
  u3_noun memwrite_bat = u3k(u3h(memwrite_script));
  u3_noun call_ext_bat = u3k(u3h(call_ext_script));
  u3_noun try_bat = u3k(u3h(try_script));
  u3_noun catch_bat = u3k(u3h(catch_script));
  u3_noun return_bat = u3k(u3h(return_script));
  u3_noun global_set_bat = u3k(u3h(global_set_script));
  u3_noun global_get_bat = u3k(u3h(global_get_script));
  u3_noun mem_grow_bat = u3k(u3h(mem_grow_script));
  u3_noun mem_size_bat = u3k(u3h(mem_size_script));

  u3_noun call_ctx = u3k(u3at(ARROW_CTX, call_script));
  u3_noun memread_ctx = u3k(u3at(ARROW_CTX, memread_script));
  u3_noun memwrite_ctx = u3k(u3at(ARROW_CTX, memwrite_script));
  u3_noun global_set_ctx = u3k(u3at(ARROW_CTX, global_set_script));
  u3_noun global_get_ctx = u3k(u3at(ARROW_CTX, global_get_script));
  u3_noun mem_grow_ctx = u3k(u3at(ARROW_CTX, mem_grow_script));
  u3_noun mem_size_ctx = u3k(u3at(MONAD_CTX, mem_grow_script));

  u3z(call_script);
  u3z(memread_script);
  u3z(memwrite_script);
  u3z(call_ext_script);
  u3z(try_script);
  u3z(catch_script);
  u3z(return_script);
  u3z(global_set_script);
  u3z(global_get_script);
  u3z(mem_grow_script);
  u3z(mem_size_script);
  
  match_data_struct match = {
    call_bat,
    memread_bat,
    memwrite_bat,
    call_ext_bat,
    try_bat,
    catch_bat,
    return_bat,
    global_set_bat,
    global_get_bat,
    mem_grow_bat,
    mem_size_bat,
  //
    call_ctx,
    memread_ctx,
    memwrite_ctx,
    global_set_ctx,
    global_get_ctx,
    mem_grow_ctx,
    mem_size_ctx,
  };

  u3_noun octs = u3at(2, seed_new);
  u3_noun p_octs, q_octs;
  u3x_cell(octs, &p_octs, &q_octs);

  c3_w bin_len_w = (c3y == u3a_is_cat(p_octs)) ? p_octs : u3m_bail(c3__fail);
  c3_y* bin_y = u3r_bytes_alloc(0, bin_len_w, u3x_atom(q_octs));

  M3Result result;

  result = m3_SetAllocators(u3a_calloc, u3a_free, u3a_realloc);

  if (result)
  {
    fprintf(stderr, ERR("set allocators fail: %s"), result);
    return u3m_bail(c3__fail);
  }

  IM3Environment wasm3_env = m3_NewEnvironment();
  if (!wasm3_env)
  {
    fprintf(stderr, ERR("env is null"));
    return u3m_bail(c3__fail);
  }
  
  // 2MB stack
  IM3Runtime wasm3_runtime = m3_NewRuntime(wasm3_env, 1 << 21, NULL);
  if (!wasm3_runtime)
  {
    fprintf(stderr, ERR("runtime is null"));
    return u3m_bail(c3__fail);
  }

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
  u3_noun monad = u3at(6, seed_new);
  u3_noun lia_shop = u3at(14, seed_new);
  u3_noun import = u3at(15, seed_new);

  lia_state sat = {wasm3_module, lia_shop, import, &match, 0};

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
    sat.arrow_yil = 0;
    if (yil == 0)
    {
      return u3m_bail(c3__fail);
    }
  }
  else if (_deterministic_trap(result))
  {
    fprintf(stderr, WUT("start function call trapped: %s"), result);
    yil = u3nc(2, 0);
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
  u3_noun pro = u3m_love(u3nc(yil, seed_new));
  #else
  u3_noun pro = u3nc(yil, seed_new);
  #endif

  return pro;
}

#endif // URWASM_STATEFUL

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

  #define KICK1(TRAP)  u3j_kink(TRAP, 2)
  #define KICK2(TRAP)  u3j_kink(KICK1(TRAP), 2)

  u3_noun runnable = u3j_kink(u3k(ctx), AX_RUNNABLE);
  u3_noun arrows   = KICK1(u3j_kink(u3k(ctx), AX_ARROWS));

  u3_noun call_script         = KICK1(u3j_kink(u3k(arrows), AX_CALL));  
  u3_noun memread_script      = KICK1(u3j_kink(u3k(arrows), AX_MEMREAD));  
  u3_noun memwrite_script     = KICK1(u3j_kink(u3k(arrows), AX_MEMWRITE));  
  u3_noun call_ext_script     = KICK1(u3j_kink(u3k(arrows), AX_CALL_EXT));
  u3_noun global_set_script   = KICK1(u3j_kink(u3k(arrows), AX_GLOBAL_SET));
  u3_noun global_get_script   = KICK1(u3j_kink(u3k(arrows), AX_GLOBAL_GET));
  u3_noun mem_grow_script     = KICK1(u3j_kink(u3k(arrows), AX_MEM_GROW));
  u3_noun mem_size_script     =       u3j_kink(u3k(arrows), AX_MEM_SIZE);
  u3_noun get_acc_script      =       u3j_kink(u3k(arrows), AX_GET_ACC);
  u3_noun set_acc_script      = KICK1(u3j_kink(u3k(arrows), AX_SET_ACC));
  u3_noun get_all_glob_script =       u3j_kink(u3k(arrows), AX_GET_ALL_GLOB);
  u3_noun set_all_glob_script = KICK1(u3j_kink(    arrows,  AX_SET_ALL_GLOB));

  u3_noun try_script    = KICK2(u3j_kink(u3k(runnable), AX_TRY));  
  u3_noun catch_script  = KICK2(u3j_kink(u3k(runnable), AX_CATCH));
  u3_noun return_script = KICK1(u3j_kink(    runnable,  AX_RETURN));
  
  u3_noun call_bat = u3k(u3h(call_script));
  u3_noun memread_bat = u3k(u3h(memread_script));
  u3_noun memwrite_bat = u3k(u3h(memwrite_script));
  u3_noun call_ext_bat = u3k(u3h(call_ext_script));
  u3_noun try_bat = u3k(u3h(try_script));
  u3_noun catch_bat = u3k(u3h(catch_script));
  u3_noun return_bat = u3k(u3h(return_script));
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

  result = m3_SetAllocators(u3a_calloc, u3a_free, u3a_realloc);

  if (result)
  {
    fprintf(stderr, ERR("set allocators fail: %s"), result);
    return u3m_bail(c3__fail);
  }

  result = m3_SetTransientAllocators(u3a_calloc, u3a_free, u3a_realloc);

  if (result)
  {
    fprintf(stderr, ERR("set allocators fail: %s"), result);
    return u3m_bail(c3__fail);
  }

  IM3Environment wasm3_env = m3_NewEnvironment();
  if (!wasm3_env)
  {
    fprintf(stderr, ERR("env is null"));
    return u3m_bail(c3__fail);
  }
  
  // 2MB stack
  IM3Runtime wasm3_runtime = m3_NewRuntime(wasm3_env, 1 << 21, NULL, 1);
  if (!wasm3_runtime)
  {
    fprintf(stderr, ERR("runtime is null"));
    return u3m_bail(c3__fail);
  }

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
  u3_noun lia_shop = u3_nul;
  u3_noun import = u3at(u3x_sam_5, cor);

  u3_noun acc, map;
  u3x_cell(import, &acc, &map);

  lia_state sat = {wasm3_module, lia_shop, u3k(acc), map, &match, u3_none, u3_nul, u3_none};

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

  result = m3_CompileModule(wasm3_module);
  if (result)
  {
    fprintf(stderr, ERR("compilation error: %s"), result);
    return u3m_bail(c3__fail);
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
