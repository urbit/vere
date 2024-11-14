/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"


const c3_w c3__i32  = c3_s3('i','3','2');
const c3_w c3__i64  = c3_s3('i','6','4');
const c3_w c3__f32  = c3_s3('f','3','2');
const c3_w c3__f64  = c3_s3('f','6','4');

const M3Result m3Lia_Arrow = "non-zero yield from import arrow";

typedef struct {
  u3_noun call_bat;
  u3_noun memread_bat;
  u3_noun memwrite_bat;
  u3_noun call_ext_bat;
  u3_noun try_bat;
  u3_noun catch_bat;
  u3_noun return_bat;
  u3_noun call_ctx;
  u3_noun memread_ctx;
  u3_noun memwrite_ctx;
} match_data_struct;

typedef struct {
  IM3Module wasm_module; // p
  u3_noun lia_shop;      // r
  u3_noun import;        // q
  match_data_struct* match;
  u3_noun arrow_yil;
} lia_state;

//  TRANSFER result
static u3_noun
_atoms_from_stack(const void** valptrs, c3_w n, c3_y* types) {
  u3_noun out = u3_nul;
  while (n) {
    n--;
    switch (types[n]) {  // TODO 64 bit vere
      case c_m3Type_i32: {
        out = u3nc(
          u3i_word(*(c3_w*)valptrs[n]), out);
        break;
      }
      case c_m3Type_i64: {
        out = u3nc(
          u3i_chub(*(c3_d*)valptrs[n]), out);
        break;
      }
      case c_m3Type_f32: {
        out = u3nc(
          u3i_word(*(c3_w*)valptrs[n]), out);
        break;
      }
      case c_m3Type_f64: {
        out = u3nc(
          u3i_chub(*(c3_d*)valptrs[n]), out);
        break;
      }
      default: return u3m_bail(c3__fail);
    }
  }
  return out;
}

//  RETAIN coins
static void
_atoms_to_stack(
  u3_noun atoms,
  const void** valptrs,
  c3_w n,
  c3_y* types
) {
  for (c3_w i = 0; i < n; i++) {
    if (c3y == u3ud(atoms)) {
      u3m_bail(c3__fail);
    }
    u3_noun atom;
    u3x_cell(atoms, &atom, &atoms);
    if (c3n == u3ud(atom)) {
      u3m_bail(c3__fail);
    }
    switch (types[i]) {
      case c_m3Type_i32: {
        *(c3_w*)valptrs[i] = u3r_word(0, atom);
        break;
      }
      case c_m3Type_i64: {
        *(c3_d*)valptrs[i] = u3r_chub(0, atom);
        break;
      }
      case c_m3Type_f32: {
        *(c3_w*)valptrs[i] = u3r_word(0, atom);
        break;
      }
      case c_m3Type_f64: {
        *(c3_d*)valptrs[i] = u3r_chub(0, atom);
        break;
      }
      default: u3m_bail(c3__fail);
    }
  }
  if (u3_nul != atoms) u3m_bail(c3__fail);
}

//  TRANSFER result
static u3_noun
_coins_from_stack(const void** valptrs, c3_w n, c3_y* types) {
  u3_noun out = u3_nul;
  while (n) {
    n--;
    switch (types[n]) {  // TODO 64 bit vere
      case c_m3Type_i32: {
        out = u3nc(u3nc(c3__i32, u3i_word(*(c3_w*)valptrs[n])), out);
        break;
      }
      case c_m3Type_i64: {
        out = u3nc(u3nc(c3__i64, u3i_chub(*(c3_d*)valptrs[n])), out);
        break;
      }
      case c_m3Type_f32: {
        out = u3nc(u3nc(c3__f32, u3i_word(*(c3_w*)valptrs[n])), out);
        break;
      }
      case c_m3Type_f64: {
        out = u3nc(u3nc(c3__f64, u3i_chub(*(c3_d*)valptrs[n])), out);
        break;
      }
      default: return u3m_bail(c3__fail);
    }
  }
  return out;
}

//  RETAIN coins
static void
_coins_to_stack(
  u3_noun coins,
  const void** valptrs,
  c3_w n,
  c3_y* types
) {
  for (c3_w i = 0; i < n; i++) {
    if (c3y == u3ud(coins)) {
      u3m_bail(c3__fail);
    }
    u3_noun coin;
    u3x_cell(coins, &coin, &coins);
    if (c3y == u3ud(coin)) {
      u3m_bail(c3__fail);
    }
    u3_noun tag, value;
    u3x_cell(coin, &tag, &value);
    if (c3n == u3ud(value)) {
      u3m_bail(c3__fail);
    }
    switch (types[i]) {
      case c_m3Type_i32: {
        if (c3__i32 != tag) u3m_bail(c3__fail);
        *(c3_w*)valptrs[i] = u3r_word(0, value);
        break;
      }
      case c_m3Type_i64: {
        if (c3__i64 != tag) u3m_bail(c3__fail);
        *(c3_d*)valptrs[i] = u3r_chub(0, value);
        break;
      }
      case c_m3Type_f32: {
        if (c3__f32 != tag) u3m_bail(c3__fail);
        *(c3_w*)valptrs[i] = u3r_word(0, value);
        break;
      }
      case c_m3Type_f64: {
        if (c3__f64 != tag) u3m_bail(c3__fail);
        *(c3_d*)valptrs[i] = u3r_chub(0, value);
        break;
      }
      default: u3m_bail(c3__fail);
    }
  }
  if (u3_nul != coins) u3m_bail(c3__fail);
}

//  RETAIN args, TRANSFER result
static u3_noun
_reduce_monad(u3_noun monad, lia_state* sat) {
  u3_noun monad_bat = u3h(monad);
  if (c3y == u3r_sing(monad_bat, sat->match->call_bat)) {
    if (c3n == u3r_sing(u3at(63, monad), sat->match->call_ctx)) {
      return u3m_bail(c3__fail);
    }
    //  call
    u3_noun name = u3at(124, monad);
    u3_noun args = u3at(125, monad);

    c3_c* name_c = u3r_string(name);
    M3Result result;

    IM3Function f;
    result = m3_FindFunction(&f, sat->wasm_module->runtime, name_c);

    if (result) return u3m_bail(c3__fail);

    c3_w n_in  = f->funcType->numArgs;
    c3_w n_out = f->funcType->numRets;
    c3_y* types = f->funcType->types;

    const c3_d *vals_in = u3a_calloc(n_in, sizeof(c3_d));  //  TO FREE
    const void **valptrs_in = u3a_calloc(n_in, sizeof(void*));  //  TO FREE
    for (c3_w i = 0; i < n_in; i++) valptrs_in[i] = &vals_in[i];

    const c3_d *vals_out = u3a_calloc(n_out, sizeof(c3_d));  //  TO FREE
    const void **valptrs_out = u3a_calloc(n_out, sizeof(void*));  //  TO FREE
    for (c3_w i = 0; i < n_out; i++) valptrs_out[i] = &vals_out[i];

    _atoms_to_stack(args, valptrs_in, n_in, (types+n_out));
    
    fprintf(stderr, "\r\nprepared call: %s\r\n", name_c);

    result = m3_Call(f, n_in, valptrs_in);  //  break pkg/noun/jets/e/urwasm.c:228

    fprintf(stderr, "\r\ncall ended\r\n");

    if (result == m3Lia_Arrow) {
      u3_noun yil = sat->arrow_yil;
      sat->arrow_yil = 0;
      if (yil == 0) return u3m_bail(c3__fail);
      return yil;
    }
    else if (result) {
      fprintf(stderr, "\r\ncall failed: %s \r\n", name_c);
      return u3m_bail(c3__fail);
    }

    result = m3_GetResults(f, n_out, valptrs_out);
    if (result) return u3m_bail(c3__fail);

    fprintf(stderr, "\r\ngetting results\r\n");

    u3_noun out = _atoms_from_stack(valptrs_out, n_out, types);
    c3_free(name_c);

    return u3nc(0, out);

  }
  else if (c3y == u3r_sing(monad_bat, sat->match->memread_bat)) {
    if (c3n == u3r_sing(u3at(63, monad), sat->match->memread_ctx)) {
      return u3m_bail(c3__fail);
    }
    //  memread
    u3_noun ptr = u3at(124, monad);
    u3_noun len = u3at(125, monad);

    u3m_p("read ptr", ptr);
    u3m_p("len", len);

    c3_l ptr_l = (c3y == u3a_is_cat(ptr))
                ? ptr
                : u3m_bail(c3__fail);
    c3_l len_l = (c3y == u3a_is_cat(len))
                ? len
                : u3m_bail(c3__fail);
    c3_w len_buf_w;
    c3_y* buf_y = m3_GetMemory(sat->wasm_module->runtime, &len_buf_w, 0);

    if ( (buf_y == NULL) || (ptr_l + len_l > len_buf_w) ) {
      return u3m_bail(c3__fail);
    }

    return u3nt(0, len_l, u3i_bytes(len_l, (buf_y + ptr_l)));

  }
  else if (c3y == u3r_sing(monad_bat, sat->match->memwrite_bat)) {
    if (c3n == u3r_sing(u3at(63, monad), sat->match->memwrite_ctx)) {
      return u3m_bail(c3__fail);
    }
    //  memwrite
    u3_noun ptr = u3at(124, monad);
    u3_noun len = u3at(250, monad);
    u3_noun src = u3at(251, monad);

    c3_l ptr_l = (c3y == u3a_is_cat(ptr))
                ? ptr
                : u3m_bail(c3__fail);
    c3_l len_l = (c3y == u3a_is_cat(len))
                ? len
                : u3m_bail(c3__fail);
    c3_w len_buf_w;
    c3_y* buf_y = m3_GetMemory(sat->wasm_module->runtime, &len_buf_w, 0);

    if ( (buf_y == NULL) || (ptr_l + len_l > len_buf_w) ) {
      return u3m_bail(c3__fail);
    }

    u3r_bytes(0, len_l, (buf_y + ptr_l), u3x_atom(src));
    
    return u3nc(0, 0);
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->call_ext_bat)) {
    //  call-ext
    u3_noun name = u3at(124, monad);
    u3_noun args = u3at(125, monad);
    if (u3_nul == sat->lia_shop) {
      return u3nt(1, u3k(name), u3k(args));
    }
    else {
      u3_noun lia_buy;
      u3x_cell(sat->lia_shop, &lia_buy, &sat->lia_shop);
      return u3nc(0, u3k(lia_buy));
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->try_bat)) {
    //  try
    u3_noun monad_b = u3at(60, monad);
    u3_noun cont = u3at(61, monad);
    
    u3_noun yil = _reduce_monad(monad_b, sat);  //  TO LOSE
    if (0 != u3h(yil)) {
      return yil;
    }
    u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil)));
    return _reduce_monad(monad_cont, sat);
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->catch_bat)) {
    //  catch
    u3_noun monad_try = u3at(120, monad);
    u3_noun monad_catch = u3at(121, monad);
    u3_noun cont = u3at(61, monad);

    u3_noun yil_try = _reduce_monad(monad_try, sat);  //  TO LOSE
    if (0 == u3h(yil_try)) {
      u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil_try)));
      return _reduce_monad(monad_cont, sat);
    }
    else if (1 == u3h(yil_try)) {
      return yil_try;
    }
    else {
      u3_noun yil_catch = _reduce_monad(monad_catch, sat);
      if (0 != u3h(yil_catch)) {
        return yil_catch;
      }
      u3_noun monad_cont = u3n_slam_on(u3k(cont), u3k(u3t(yil_catch)));
      return _reduce_monad(monad_cont, sat);
    }
  }
  else if (c3y == u3r_sing(monad_bat, sat->match->return_bat)) {
    //  return
    return u3nc(0, u3at(30, monad));
  }
  else {
    return u3m_bail(c3__fail);
  }
}

static const void *
_link_wasm_with_arrow_map(
  IM3Runtime king_runtime,
  IM3ImportContext _ctx,
  uint64_t * _sp,
  void * _mem
) {
  const char *mod = _ctx->function->import.moduleUtf8;
  const char *name = _ctx->function->import.fieldUtf8;
  lia_state* sat = _ctx->userdata;

  M3Result result;

  u3_noun key = u3nc(u3i_string(mod), u3i_string(name));
  u3_noun arrow = u3kdb_got(u3k(sat->import), key);
  c3_w n_in  = _ctx->function->funcType->numArgs;
  c3_w n_out = _ctx->function->funcType->numRets;
  c3_y* types = _ctx->function->funcType->types;
  const void **valptrs_in = u3a_calloc(n_in, sizeof(void*)); // TO FREE
  for (c3_w i = 0; i < n_in; i++) {
      valptrs_in[i] = &_sp[i+n_out];
    }
  const void **valptrs_out = u3a_calloc(n_out, sizeof(void*)); // TO FREE
  for (c3_w i = 0; i < n_out; i++) {
      valptrs_out[i] = &_sp[i];
    }
  
  u3_noun coin_wasm_list = _coins_from_stack(valptrs_in, n_in, (types+n_out));
  
  u3_noun yil = _reduce_monad(u3n_slam_on(arrow, coin_wasm_list), sat); // TO LOSE

  if (0 != u3h(yil)) {
    sat->arrow_yil = yil;
    return m3Lia_Arrow;
  } else {
    _coins_to_stack(u3t(yil), valptrs_out, n_out, types);
    return m3Err_none;
  }
}

u3_weak
u3we_lia_run(u3_noun cor)
{
  fprintf(stderr, "\r\njet entry\r\n");

  if (c3__none == u3x_at(u3x_sam_7, cor)) {
    return u3_none;
  }

  //  enter subroad, 4MB safety buffer
  // u3m_hate(1 << 20);
    
  u3_noun input = u3x_at(u3x_sam_2, cor);
  u3_noun seed = u3x_at(u3x_sam_6, cor);
  
  u3_noun runnable = u3j_kink(u3k(u3at(7, cor)), 372);
  u3_noun try_gate = u3j_kink(u3k(runnable), 21);

  u3_noun seed_new;  //  TO RETURN
  u3_noun input_tag, p_input;
  u3x_cell(input, &input_tag, &p_input);

  if (input_tag == c3y) {
    u3_noun p_input_gate = u3nt(u3nc(0, 7), 0, u3k(p_input));  //  =>(p.input |=(* +>))
    u3_noun past_new = u3n_slam_on(
      u3j_kink(u3k(try_gate), 2),
      u3nc(
        u3k(u3x_at(6, seed)),
        p_input_gate
      )
    );
    seed_new = u3nq(
      u3k(u3x_at(2, seed)),
      past_new,
      u3k(u3x_at(14, seed)),
      u3k(u3x_at(15, seed))
    );
  }
  else if (input_tag == c3n) {
    seed_new = u3nq(
      u3k(u3x_at(2, seed)),
      u3x_at(6, seed),
      u3kb_weld(
        u3k(u3x_at(14, seed)),
        u3nc(u3k(p_input), u3_nul)
      ),
      u3k(u3x_at(15, seed))
    );
  }
  else {
    return u3m_bail(c3__fail);
  }

  u3_noun call_script = u3j_kink(u3j_kink(u3k(u3at(7, cor)), 20), 2);  //  TO LOSE
  u3_noun memread_script = u3j_kink(u3j_kink(u3k(u3at(7, cor)), 374), 2);  //  TO LOSE
  u3_noun memwrite_script = u3j_kink(u3j_kink(u3k(u3at(7, cor)), 92), 2);  //  TO LOSE
  u3_noun call_ext_script = u3j_kink(u3j_kink(u3k(u3at(7, cor)), 2986), 2);  //  TO LOSE
  u3_noun try_script = u3j_kink(u3j_kink(try_gate, 2), 2);  //  TO LOSE
  u3_noun catch_script = u3j_kink(u3j_kink(u3j_kink(u3k(runnable), 4), 2), 2);  //  TO LOSE
  u3_noun return_script = u3j_kink(u3j_kink(runnable, 20), 2);  //  TO LOSE
  
  u3_noun call_bat = u3h(call_script);
  u3_noun memread_bat = u3h(memread_script);
  u3_noun memwrite_bat = u3h(memwrite_script);
  u3_noun call_ext_bat = u3h(call_ext_script);
  u3_noun try_bat = u3h(try_script);
  u3_noun catch_bat = u3h(catch_script);
  u3_noun return_bat = u3h(return_script);

  u3_noun call_ctx = u3at(63, call_script);
  u3_noun memread_ctx = u3at(63, memread_script);
  u3_noun memwrite_ctx = u3at(63, memwrite_script);

  match_data_struct match = {
    call_bat,
    memread_bat,
    memwrite_bat,
    call_ext_bat,
    try_bat,
    catch_bat,
    return_bat,
    call_ctx,
    memread_ctx,
    memwrite_ctx
  };

  u3_noun octs = u3x_at(2, seed_new);
  u3_noun p_octs, q_octs;
  u3x_cell(octs, &p_octs, &q_octs);

  c3_w bin_len_w = (c3y == u3a_is_cat(p_octs)) ? p_octs : u3m_bail(c3__fail);
  c3_y* bin_y = u3r_bytes_alloc(0, bin_len_w, u3x_atom(q_octs));

  m3_SetAllocators(u3a_calloc, u3a_free, u3a_realloc);

  IM3Environment wasm3_env = m3_NewEnvironment();
  if (!wasm3_env) {
    fprintf(stderr, "env is null\r\n");
    return u3m_bail(c3__fail);
  }
  M3Result result;
  
  fprintf(stderr, "\r\nstart initialization\r\n");
  
  IM3Runtime wasm3_runtime = m3_NewRuntime(wasm3_env, 1 << 13, NULL);
  if (!wasm3_runtime) {
    fprintf(stderr, "runtime is null\r\n");
    return u3m_bail(c3__fail);
  }

  IM3Module wasm3_module;
  result = m3_ParseModule(wasm3_env,
                          &wasm3_module,
                          bin_y,
                          bin_len_w);
  if (result) {
    fprintf(stderr, "parse binary error: %s\r\n", result);
    return u3m_bail(c3__fail);
  }

  //  TODO validate

  result = m3_LoadModule(wasm3_runtime, wasm3_module);
  if (result) {
    fprintf(stderr, "load module error: %s\r\n", result);
    return u3m_bail(c3__fail);
  }

  c3_w n_imports = wasm3_module->numFuncImports;
  u3_noun monad = u3x_at(6, seed_new);
  u3_noun lia_shop = u3x_at(14, seed_new);
  u3_noun import = u3x_at(15, seed_new);

  lia_state sat = {wasm3_module, lia_shop, import, &match, 0};

  for (c3_w i = 0; i < n_imports; i++) {
    M3Function f = wasm3_module->functions[i];
    const char * mod  = f.import.moduleUtf8;
    const char * name = f.import.fieldUtf8;
    result = m3_LinkRawFunctionEx(wasm3_module,
                                  mod, name, NULL,
                                  &_link_wasm_with_arrow_map,
                                  (void *)&sat);
    if (result) {
      fprintf(stderr, "link error");
      return u3m_bail(c3__fail);
    }
  }
  fprintf(stderr, "\r\ninitialized state\r\n");

  u3_noun yil = _reduce_monad(monad, &sat);

  
  // //  exit subroad, copying the result
  // u3_noun pro = u3m_love(u3nc(yil, seed_new));
  u3_noun pro = u3nc(yil, seed_new);

  return pro;
}
