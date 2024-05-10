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
u3qa_lia_main(u3_noun module,
              u3_noun code_vals,
              u3_noun shop,
              u3_noun ext,
              u3_noun import,
              u3_noun diff,
              u3_atom hint)
{
  fprintf(stderr, "\r\nJET HIT\r\n"); 
  return u3_none;
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
    return u3qa_lia_main(module, code_vals, shop, ext, import, diff, hint);
  }
}