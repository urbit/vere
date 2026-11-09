/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "nock-compile.h"
#include "noun.h"

/* _fu_nest_dext(): the memoized arm, a miss run by fun_f.
*/
static u3_noun
_fu_nest_dext(u3_noun dext_core, u3_funq fun_f)
{
  u3_noun nest_in_core, nest_core;
  u3_noun bat, sut, ref, van, seg, reg, gil;

  if (  (u3_none == (nest_in_core = u3r_at(3, dext_core)))
     || (c3n == u3r_mean(nest_in_core, {u3x_sam_2, &seg},
                                       {u3x_sam_6, &reg},
                                       {u3x_sam_7, &gil},
                                       {u3x_con, &nest_core}))
     || (c3n == u3r_mean(nest_core, {u3x_sam_3, &ref},
                                    {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (sut = u3r_at(u3x_sam, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    u3_weak vet = u3r_at(u3qfu_van_vet, van);
    c3_m  fun_m = 141 + c3__dext + ((!!vet) << 8);
    u3_noun key = u3z_key_3(fun_m, sut, ref, bat);
    u3_weak pro = u3z_find(u3z_memo_toss, key);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      pro = fun_f(u3k(dext_core), u3k(u3x_at(u3x_bat, dext_core)));

      if ( ((c3y == pro) && (u3_nul == reg)) ||
           ((c3n == pro) && (u3_nul == seg)) )
      {
        return u3z_save(u3z_memo_toss, key, pro);
      }
      else {
        u3z(key);
        return pro;
      }
    }
  }
}

/* u3wfu_nest_dext(): from u3n, in u3n.
*/
u3_noun
u3wfu_nest_dext(u3_noun dext_core)
{
  return _fu_nest_dext(dext_core, u3n_nock_on);
}

/* u3ufu_nest_dext(): from compiled nock, in compiled nock.
*/
u3_noun
u3ufu_nest_dext(u3_noun dext_core)
{
  return _fu_nest_dext(dext_core, u3nc_gop_nock);
}
