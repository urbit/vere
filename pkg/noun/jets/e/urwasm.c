/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"

// #include <stdio.h>
// #include <math.h>

const char* N_FUNCS        = "n-funcs";
const char* SET_I32        = "set-i32";
const char* SET_I64        = "set-i64";
const char* SET_F32        = "set-f32";
const char* SET_F64        = "set-f64";
const char* CLEAR_SPACE    = "clear-space";
const char* SET_OCTS_EXT   = "set-octs-ext";
const char* GET_SPACE_PTR  = "get-space-ptr";
const char* ACT_0_FUNC_IDX = "act-0-func-idx";

const c3_w TAS_I32  = c3_s3('i','3','2');
const c3_w TAS_I64  = c3_s3('i','6','4');
const c3_w TAS_F32  = c3_s3('f','3','2');
const c3_w TAS_F64  = c3_s3('f','6','4');
const c3_w TAS_V128 = c3_s4('v','1','2', '8');
const c3_w TAS_OCTS = c3_s4('o','c','t', 's');

const c3_ws MINUS_ONE = -1;

static u3_noun
_get_results_wasm(IM3Function i_function, c3_w i_retc)
{
  IM3FuncType ftype = i_function->funcType;
  IM3Runtime runtime = i_function->module->runtime;

  if (i_retc != ftype->numRets) {
      return u3m_bail(c3__exit);
  }
  if (i_function != runtime->lastCalled) {
      return u3m_bail(c3__fail);
  }
  c3_y* s = (c3_y*) runtime->stack;
  u3_noun list_out = u3_nul;
  for (c3_w i = 0; i < ftype->numRets; ++i) {
    switch (d_FuncRetType(ftype, i)) {
    case c_m3Type_i32: {
      c3_w out = *(c3_w*)(s);
      s += 8;
      list_out = u3nc(u3nc(TAS_I32, u3i_word(out)), list_out);
      break;
    }
    case c_m3Type_i64: {
      c3_d out = *(c3_d*)(s);
      s += 8;
      list_out = u3nc(u3nc(TAS_I64, u3i_chub(out)), list_out);
      break;
    }
    case c_m3Type_f32: {
      c3_w out = *(c3_w*)(s);
      s += 8;
      list_out = u3nc(u3nc(TAS_F32, u3i_word(out)), list_out);
      break;
    }
    case c_m3Type_f64: {
      c3_d out = *(c3_d*)(s);
      s += 8;
      list_out = u3nc(u3nc(TAS_F64, u3i_chub(out)), list_out);
      break;
    }
    default: return u3m_bail(c3__fail);
    }
  }
  return u3kb_flop(list_out);
}

static c3_o
_put_space(u3_cell val, IM3Runtime runtime, c3_w target)
{
  u3_noun type, content;
  u3x_cell(val, &type, &content);
  M3Result result;
  switch (type) {
    default: {
      return c3n;
    }
    case TAS_I32: {
      IM3Function f;
      result = m3_FindFunction(&f, runtime, SET_I32);
      if (result) {
        return c3n;
      }
      result = m3_CallV(f, u3r_word(0, content), target);
      if (result) {
        fprintf(stderr, "call i32 error: %s\r\n", result);
        return c3n;
      }
      break;
    }
    case TAS_I64: {
      IM3Function f;
      result = m3_FindFunction(&f, runtime, SET_I64);
      if (result) {
        return c3n;
      }
      result = m3_CallV(f, u3r_chub(0, content), target);
      if (result) {
        fprintf(stderr, "call i64 error: %s\r\n", result);
        return c3n;
      }
      break;
    }
    case TAS_F32: {
      IM3Function f;
      result = m3_FindFunction (&f, runtime, SET_F32);
      if (result) {
        return c3n;
      }
      result = m3_CallV(f, u3r_word(0, content), target);
      if (result) {
        fprintf(stderr, "call f32 error: %s\r\n", result);
        return c3n;
      }
      break;
    }
    case TAS_F64: {
      IM3Function f;
      result = m3_FindFunction(&f, runtime, SET_F64);
      if (result) {
        return c3n;
      }
      result = m3_CallV(f, u3r_chub(0, content), target);
      if (result) {
        fprintf(stderr, "call f64 error: %s\r\n", result);
        return c3n;
      }
      break;
    }
    case TAS_V128: {
      return c3n; // wasm3 can't accept v128 as an input
    }
    case TAS_OCTS: {
      IM3Function f;
      result = m3_FindFunction (&f, runtime, SET_OCTS_EXT);
      if (result) {
        return c3n;
      }
      u3_atom octs_len, octs_data;
      u3x_cell(content, &octs_len, &octs_data);
      c3_w w_octs_len = u3r_word(0, octs_len);
      result = m3_CallV(f, w_octs_len, target);
      if (result) {
        fprintf(stderr, "call set-octs-ext error: %s\r\n", result);
        return c3n;
      }
      u3_noun list_ptr =  _get_results_wasm(f, 1);
      if (u3h(u3h(list_ptr)) != TAS_I32) {
        return c3n;
      }
      c3_w ptr = u3r_word(0, u3t(u3h(list_ptr)));
      u3z(list_ptr);
      if (ptr == MINUS_ONE) {
        if (c3n == u3r_sing_cell(0, 0, content)) {
          return c3n;
        }
      }
      else {
        c3_w len_mem;
        c3_y* mem = m3_GetMemory(runtime, &len_mem, 0);
        if ( (mem == NULL) || (ptr + w_octs_len > len_mem) ) {
          return u3m_bail(c3__fail);
        }
        u3r_bytes(0, w_octs_len, (c3_y *)(mem+ptr), octs_data);
      }
      break;
    }
  }
  return c3y;
}

u3_weak
u3wa_lia_main(u3_noun cor)
{
  u3_noun hint = u3x_at(u3x_sam_127, cor);
  u3_noun serf_octs = u3x_at(u3x_sam_2, cor);
  if ( (c3n == u3ud(hint)) || (c3n == u3du(serf_octs)) ) {
    return u3m_bail(c3__exit);
  }
  fprintf(stderr, "\r\nJET HIT\r\n");
  if ( (c3y == u3a_is_cat(hint)) &&
       (hint == c3__none) )
  {
    return u3_none;
  }
  else {
    // main:encoder  [7 [9 2 0 31] 9 1.524 0 1]
    // main:line     [7 [9 2 0 127] 9 10 0 1]
    // main:comp     [7 [9 2 0 63] 9 22 0 1]
    u3_noun core_line, core_encoder, core_comp;
    u3_noun gate_line, gate_encoder, gate_comp;
    u3_noun input_line_vals, input_line, vals;
    u3_noun line_module, line_code, line_shop,
            line_ext, line_import, line_diff;
    u3_noun king_ast, king_octs;

    core_encoder = u3j_kink(u3k(u3at(31, cor)),  2);
    core_line    = u3j_kink(u3k(u3at(127, cor)), 2);
    core_comp    = u3j_kink(u3k(u3at(63, cor)),  2);

    gate_encoder = u3j_kink(core_encoder, 1524);
    gate_line =    u3j_kink(core_line,    10);
    gate_comp =    u3j_kink(core_comp,    22);

    input_line_vals = u3n_slam_on(gate_line, u3k(u3at(u3x_sam, cor)));
    u3x_cell(input_line_vals, &input_line, &vals);
    u3x_mean(input_line, 2, &line_module,
                         6  &line_code,
                        14  &line_shop,
                        30  &line_ext,
                        62  &line_import,
                        63  &line_diff,
                        0);
    if (c3n == u3ud(line_diff))
    {
      u3_noun flag, p_diff;
      u3x_cell(line_diff, &flag, &p_diff);
      u3k(line_code);
      u3k(line_shop);
      if (c3y == flag) {
        line_code = u3kb_weld(line_code, u3nc(u3k(p_diff), u3_nul));
      }
      else {
        line_shop = u3kb_weld(line_shop, u3nc(u3k(p_diff), u3_nul));
      }
    }
    king_ast = u3n_slam_on(gate_comp,
      u3i_qual(u3k(line_module),
              line_code,
              u3k(line_ext),
              u3k(line_import)
              )
    );
    king_octs = u3n_slam_on(gate_encoder, king_ast);
    u3_atom king_len = u3h(king_octs);
    if (c3n == u3a_is_cat(king_len)) {
      return u3m_bail(c3__fail);
    }
    u3_atom serf_len = u3h(serf_octs);
    if (c3n == u3a_is_cat(serf_len)) {
      return u3m_bail(c3__fail);
    }
    c3_y *king_bytes = u3r_bytes_alloc(0, king_len, u3t(king_octs));
    c3_y *serf_bytes = u3r_bytes_alloc(0, serf_len, u3t(serf_octs));

    IM3Environment wasm3_env = m3_NewEnvironment();
    if (!wasm3_env) {
      fprintf(stderr, "env is null\r\n");
      return u3m_bail(c3__fail);
    }
    
    IM3Runtime wasm3_runtime_king = m3_NewRuntime(wasm3_env, 2097152, NULL);
    if (!wasm3_runtime_king) {
      fprintf(stderr, "runtime is null\r\n");
      return u3m_bail(c3__fail);
    }
    M3Result result;
    IM3Module wasm3_module_king;
    result = m3_ParseModule(wasm3_env,
                            &wasm3_module_king,
                            king_bytes,
                            king_len);
    if (result) {
      fprintf(stderr, "parse module error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }

    result = m3_LoadModule(wasm3_runtime_king, wasm3_module_king);
    if (result) {
      fprintf(stderr, "load module error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }

    IM3Runtime wasm3_runtime_serf = m3_NewRuntime(wasm3_env, 2097152, NULL);
    if (!wasm3_runtime_serf) {
      fprintf(stderr, "runtime is null\r\n");
      return u3m_bail(c3__fail);
    }

    IM3Module wasm3_module_serf;
    result = m3_ParseModule(wasm3_env,
                                     &wasm3_module_serf,
                                     serf_bytes,
                                     serf_len);
    if (result) {
      fprintf(stderr, "parse module error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }

    result = m3_LoadModule(wasm3_runtime_serf, wasm3_module_serf);
    if (result) {
      fprintf(stderr, "load module error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }

    M3TaggedValue tagged_act_0, tagged_n_funcs;
    c3_w i32_act_0, i32_n_funcs;
    IM3Global global_act_0   = m3_FindGlobal(wasm3_runtime_king->modules, ACT_0_FUNC_IDX);
    IM3Global global_n_funcs = m3_FindGlobal(wasm3_runtime_king->modules, N_FUNCS);
    result = m3_GetGlobal (global_act_0, &tagged_act_0);
    if (result) {
      return u3m_bail(c3__fail);
    }
    result = m3_GetGlobal(global_n_funcs, &tagged_n_funcs);
    if (result) {
      return u3m_bail(c3__fail);
    }
    switch (tagged_act_0.type) {
      default: {
        return u3m_bail(c3__fail);
      }

      case c_m3Type_i32:  {
        i32_act_0 = tagged_act_0.value.i32;
      }
    }
    switch (tagged_n_funcs.type) {
      default: {
        return u3m_bail(c3__fail);
      }
      
      case c_m3Type_i32:  {
        i32_n_funcs = tagged_n_funcs.value.i32;
      }
    }
    u3_atom len_vals = u3kb_lent(u3k(vals));
    if (c3n == u3a_is_cat(len_vals)) {
      return u3m_bail(c3__fail);
    }
    if ( (0 == i32_n_funcs) || (i32_n_funcs != len_vals) ) {
      return u3m_bail(c3__fail);
    }
    c3_w func_idx_last = i32_act_0 + i32_n_funcs - 1;
    for (c3_w i_w = i32_act_0; i_w < func_idx_last; i_w++) {
      u3_noun i_vals;
      u3x_cell(vals, &i_vals, &vals);
      u3_atom len_i_vals = u3kb_lent(u3k(i_vals));
      if (c3n == u3a_is_cat(len_i_vals)) {
        return u3m_bail(c3__fail);
      }
      for (c3_w target = 0; target < len_i_vals; target++) {
        u3_noun i_i_vals;
        u3x_cell(i_vals, &i_i_vals, &i_vals);
        if (c3n == _put_space(i_i_vals, wasm3_runtime_king, target))
        {
          return u3m_bail(c3__fail);
        }
      }
      if (i_vals != u3_nul) {
        return u3m_bail(c3__fail);
      }
      IM3Function f = Module_GetFunction(wasm3_module_king, i_w);
      CompileFunction(f);
      result = m3_CallV(f);
      if (result) {
        fprintf(stderr, "call action error: %s\r\n", result);
        return u3m_bail(c3__fail);
      }
      u3z(len_i_vals);
      IM3Function f_clear;
      result = m3_FindFunction(&f_clear, wasm3_runtime_king, CLEAR_SPACE);
      if (result) {
        return u3m_bail(c3__fail);
      }
      result = m3_CallV(f_clear);
      if (result) {
        return u3m_bail(c3__fail);
      }
    }
    u3_noun i_vals;
    u3x_cell(vals, &i_vals, &vals);
    if (vals != u3_nul) {
      return u3m_bail(c3__fail);
    }
    u3_atom len_i_vals = u3kb_lent(u3k(i_vals));
    if (c3n == u3a_is_cat(len_i_vals)) {
      return u3m_bail(c3__fail);
    }
    for (c3_w target = 0; target < len_i_vals; target++) {
      u3_noun i_i_vals;
      u3x_cell(i_vals, &i_i_vals, &i_vals);
      if (c3n == _put_space(i_i_vals, wasm3_runtime_king, target)) {
        return u3m_bail(c3__fail);
      }
    }
    if (i_vals != u3_nul) {
      return u3m_bail(c3__fail);
    }
    u3z(len_i_vals);
    IM3Function f = Module_GetFunction(wasm3_module_king, func_idx_last);
    CompileFunction(f);
    result = m3_CallV(f);
    if (result) {
      fprintf(stderr, "call last action error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }
    u3_noun last_action = u3h(u3qb_flop(line_code));
    u3_noun lia_types = u3t(u3h(last_action));
    c3_w n_out = u3r_word(0,u3qb_lent(lia_types));
    u3_noun out_wasm = _get_results_wasm(f, n_out);
    u3_noun out_lia = u3_nul;
    for (c3_w i = 0; i < n_out; i++) {
      u3_noun lia_type, wasm_noun;
      u3x_cell(lia_types, &lia_type, &lia_types);
      u3x_cell(out_wasm, &wasm_noun, &out_wasm);
      if (lia_type != TAS_OCTS) {
        if (lia_type != u3h(wasm_noun)) {
          return u3m_bail(c3__exit);
        }
        out_lia = u3nc(wasm_noun, out_lia);
      }
      else {
        if (TAS_I32 != u3h(wasm_noun)) {
          return u3m_bail(c3__exit);
        }
        IM3Function f;
        result = m3_FindFunction (&f, wasm3_runtime_king, GET_SPACE_PTR);
        if (result) {
          return u3m_bail(c3__fail);
        }
        result = m3_CallV(f, u3r_word(0, u3t(wasm_noun)));
        if (result) {
          fprintf(stderr, "call GET_SPACE_PTR error: %s\r\n", result);
          return u3m_bail(c3__fail);
        }
        u3_noun list_ptr =  _get_results_wasm(f, 1);
        if (u3h(u3h(list_ptr)) != TAS_I32) {
          return u3m_bail(c3__fail);
        }
        c3_w ptr = u3r_word(0, u3t(u3h(list_ptr)));
        u3z(list_ptr);
        if (ptr == 0) {
          return u3m_bail(c3__exit);
        }
        if (ptr == MINUS_ONE) {
          out_lia = u3nc(u3nt(TAS_OCTS, 0, 0), out_lia);
        }
        else {
          c3_w len_mem;
          c3_y* mem = m3_GetMemory(wasm3_runtime_king, &len_mem, 0);
          if ( (mem == NULL) || (ptr + 4 > len_mem) ) {
            return u3m_bail(c3__fail);
          }
          u3_atom len_octs = u3i_bytes(4, (mem+ptr));
          c3_w w_len_octs = u3r_word(0, len_octs);
          if (ptr + 4 + w_len_octs > len_mem) {
            return u3m_bail(c3__fail);
          }
          u3_atom data_octs = u3i_bytes(w_len_octs, (mem+(ptr+4)));
          out_lia = u3nc(u3nt(TAS_OCTS, len_octs, data_octs), out_lia);
        }
      }
    }
    u3z(len_vals);
    u3z(input_line_vals);
    u3z(line_shop);
    u3z(king_octs);
    u3z(out_wasm);
    return u3nc(0, u3kb_flop(out_lia));
  }
}