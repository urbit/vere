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
u3qa_lia_main(u3_noun cor,
              u3_noun module,
              u3_noun code_vals,
              u3_noun shop,
              u3_noun ext,
              u3_noun import,
              u3_noun diff,
              u3_atom hint
              )
{
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
    u3_noun king_ast, king_octs;
    core_encoder = u3j_kink(u3k(u3at(31, cor)),  2);
    core_line    = u3j_kink(u3k(u3at(127, cor)), 2);
    core_comp    = u3j_kink(u3k(u3at(63, cor)), 2);
    gate_encoder = u3j_kink(core_encoder, 1524);
    gate_line =    u3j_kink(core_line,    10);
    gate_comp =    u3j_kink(core_comp,    22);
    input_line_vals = u3n_slam_on(gate_line, u3k(u3at(u3x_sam, cor)));
    u3x_cell(input_line_vals, &input_line, &vals);
    if (c3n == u3ud(diff))  //  snoc from jet w/ u3kb_weld
    {
      return u3m_bail(c3__exit);
    }
    king_ast = u3n_slam_on(gate_comp, u3i_qual(
      u3k(u3at(2, input_line)),   //  module
      u3k(u3at(6, input_line)),   //  code
      u3k(u3at(30, input_line)),  //  ext
      u3k(u3at(62, input_line)),  //  import
      )
    );
    king_octs = u3n_slam_on(gate_encoder, king_ast);
  }

}

u3_weak
u3wa_lia_main(u3_noun cor)
{
  u3_noun module, code_vals, shop, ext, import, diff, hint;
  if ( (c3n == u3r_mean(cor, u3x_sam_2,   &module,
                             u3x_sam_6,   &code_vals,
                             u3x_sam_14,  &shop,
                             u3x_sam_30,  &ext,
                             u3x_sam_62,  &import,
                             u3x_sam_126, &diff,
                             u3x_sam_127, &hint,
                             0)) ||
        (c3n == u3ud(hint)) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qa_lia_main(cor, module, code_vals, shop,
                         ext, import, diff, hint);
  }
}