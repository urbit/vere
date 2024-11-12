/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "wasm3.h"
#include "m3_env.h"

u3_weak
u3we_lia_run(u3_noun cor)
{
  if (c3__none == u3x_at(u3x_sam_7, cor)) {
    return u3_none;
  }
  
  //    prepare batteries and contexts for comparisons
  //    update seed (stateless phase)
  //    parse wasm3
  //    TODO validate wasm3
  //    instantiate wasm3
  //    statefully reduce the monad by matching
  //  the batteries and checking the contexts if necessary (call, memread, memwrite)
  //    return the final yield and the updated seed
  u3_noun tar_mold = u3nt(u3nq(8, u3nc(0, 6), 8, u3nt(u3nq(5, u3nc(0, 14), 0, 2), 0, 6)), 0, 0);  // *:~
  return u3_none;
}
