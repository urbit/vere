/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"

// #include <stdio.h>
// #include <math.h>

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

    input_line_vals = u3n_slam_on(gate_line, u3k(u3at(u3x_sam, cor)));  // u3z!
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
      if (c3y == flag) {
        line_code = u3kb_weld(line_code, u3nc(u3k(p_diff), u3_nul))
      }
      else {
        line_shop = u3kb_weld(u3k(line_shop), u3nc(u3k(p_diff), u3_nul)) // u3z!
      }
    }
    king_ast = u3n_slam_on(gate_comp,
      u3i_qual(u3k(line_module),
              line_code,
              u3k(line_ext),
              u3k(line_import)
              )
    );
    king_octs = u3n_slam_on(gate_encoder, king_ast); // u3z!
    //  TODO octs to an appropriate type for wasm3
    //  wasm3 TODO:
    //    - how to handle function imports?
    //    - how to read global values?
    //    - nouns-wasm3 datatypes marshalling
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

    // Ignoring imports for now
    //
    // 1. Instantiate king [X]
    // 2. Instantiate serf [X]
    // 3. Get globals 'act-0-func-idx' and 'n-funcs', assert 'n-funcs' > 0 [X]
    // 4. Assert (lent vals) == 'n-funcs' [X]
    // 5. for i = 'act-0-func-idx', i++,  i < ('act-0-func-idx' + 'n-funcs' - 1):
    //      place args, invoke i, ignore results [ ]
    // 6. place args, invoke ('act-0-func-idx' + 'n-funcs' - 1) [ ]
    // 7. Extract results [ ]
    //
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

    IM3Module wasm3_module_king;
    M3Result result = m3_ParseModule(wasm3_env,
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
    M3Result result = m3_ParseModule(wasm3_env,
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
    const char* ACT_0_FUNC_IDX = "act-0-func-idx";
    const char* N_FUNCS      = "n-funcs";
    const char* SET_I32      = "set-i32";
    const char* SET_I64      = "set-i64";
    const char* SET_F32      = "set-f32";
    const char* SET_F64      = "set-f64";
    const char* SET_OCTS_EXT = "set-octs-ext";
    const c3_w TAS_I32  = c3_s3('i','3','2');
    const c3_w TAS_I64  = c3_s3('i','6','4');
    const c3_w TAS_F32  = c3_s3('f','3','2');
    const c3_w TAS_F64  = c3_s3('f','6','4');
    const c3_w TAS_V128 = c3_s4('v','1','2', '8');
    const c3_w TAS_OCTS = c3_s4('o','c','t', 's');

    M3TaggedValue tagged_act_0, tagged_n_funcs;
    c3_w i32_act_0, i32_n_funcs;
    IM3Global global_act_0   = m3_FindGlobal(wasm3_runtime_king->modules, ACT_0_FUNC_IDX);
    IM3Global global_n_funcs = m3_FindGlobal(wasm3_runtime_king->modules, N_FUNCS);
    M3Result err = m3_GetGlobal (global_act_0, &tagged_act_0);
    if (err) {
      return u3m_bail(c3__fail);
    }
    M3Result err = m3_GetGlobal(global_n_funcs, &tagged_n_funcs);
    if (err) {
      return u3m_bail(c3__fail);
    }
    switch (tagged_act_0.type) {
      default: {
        return u3m_bail(c3__fail);
      }

      case c_m3Type_i32:  {
        i32_act_0 = tagged_act_0.value.i32
      }
    }
    switch (tagged_n_funcs.type) {
      default: {
        return u3m_bail(c3__fail);
      }
      
      case c_m3Type_i32:  {
        i32_n_funcs = tagged_n_funcs.value.i32
      }
    }
    u3_atom len_vals = u3kb_lent(u3k(vals)); // u3z!
    if (c3n == u3a_is_cat(len_vals)) {
      return u3m_bail(c3__fail);
    }
    if ( (0 == i32_n_funcs) || (i32_n_funcs != len_vals) ) {
      return u3m_bail(c3__fail);
    }
    c3_w func_idx_last = i32_act_0 + i32_n_funcs - 1;
    for (c3_w i_w = i32_act_0; i < func_idx_last; i_w++) {
      u3_noun i_vals;
      u3x_cell(vals, &i_vals, &vals);
      u3_atom len_i_vals = u3kb_lent(u3k(i_vals)); // u3z!
      if (c3n == u3a_is_cat(len_i_vals)) {
        return u3m_bail(c3__fail);
      }
      for (c3_w target = 0; target < len_i_vals; target++) {
        u3_noun i_i_vals;
        u3x_cell(i_vals, &i_i_vals, &i_vals);
        switch (u3h(i_i_vals)) {
          default: {
            return u3m_bail(c3__fail);
          }

          case TAS_I32:{
            IM3Function f;
            result = m3_FindFunction (&f, wasm3_runtime_king, SET_I32);
            if (result) {
              return u3m_bail(c3__fail);
            }
            result = m3_CallV(f, u3r_word(0, u3t(i_i_vals)), target);
            if (result) {
              fprintf(stderr, "call i32 error: %s\r\n", result);
              return u3m_bail(c3__fail);
            }
          }
          case TAS_I64:{
            IM3Function f;
            result = m3_FindFunction (&f, wasm3_runtime_king, SET_I64);
            if (result) {
              return u3m_bail(c3__fail);
            }
            result = m3_CallV(f, u3r_word(0, u3t(i_i_vals)), target);
            if (result) {
              fprintf(stderr, "call i64 error: %s\r\n", result);
              return u3m_bail(c3__fail);
            }
          }
          case TAS_F32:{
            IM3Function f;
            result = m3_FindFunction (&f, wasm3_runtime_king, SET_F32);
            if (result) {
              return u3m_bail(c3__fail);
            }
            result = m3_CallV(f, u3r_word(0, u3t(i_i_vals)), target);
            if (result) {
              fprintf(stderr, "call f32 error: %s\r\n", result);
              return u3m_bail(c3__fail);
            }
          }
          case TAS_F64:{
            IM3Function f;
            result = m3_FindFunction (&f, wasm3_runtime_king, SET_F64);
            if (result) {
              return u3m_bail(c3__fail);
            }
            result = m3_CallV(f, u3r_word(0, u3t(i_i_vals)), target);
            if (result) {
              fprintf(stderr, "call f64 error: %s\r\n", result);
              return u3m_bail(c3__fail);
            }
          }
          case TAS_V128:{
            return u3m_bail(c3__fail); // wasm3 can't accept v128 as an input
          }
          case TAS_OCTS:{
            IM3Function f;
            result = m3_FindFunction (&f, wasm3_runtime_king, SET_OCTS_EXT);
            if (result) {
              return u3m_bail(c3__fail);
            }
            result = m3_CallV(f, u3r_word(0, u3h(u3t(i_i_vals))), target);
            if (result) {
              fprintf(stderr, "call set-octs-ext error: %s\r\n", result);
              return u3m_bail(c3__fail);
            }
            c3_w len_mem;
            c3_y* mem = m3_GetMemory(wasm3_runtime_king, &len_mem, 0);
            //  extract ptr from result, write to memory
          }
        }
      }
      IM3Function f = Module_GetFunction(wasm3_module_king, i_w);
      CompileFunction(f);
      result = m3_CallV(f);
      if (result) {
        fprintf(stderr, "call action error: %s\r\n", result);
        return u3m_bail(c3__fail);
      }
    }
    //  place arguments in space
    IM3Function f = Module_GetFunction(wasm3_module_king, func_idx_last);
    CompileFunction(f);
    result = m3_CallV(f);
    if (result) {
      fprintf(stderr, "call last action error: %s\r\n", result);
      return u3m_bail(c3__fail);
    }
    // extract results
  }
}