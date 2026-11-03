/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/*  u3wfu_rest(): memoizing wrapper for +rest:ut.
**
**    +rest expands a hold: it crashes (rest-loop) if the hold is already
**    being expanded on the current path, i.e. is in fan, then plays its
**    body under fan extended with it.  The result depends on neither sut
**    nor vet, which +play replaces, so the key is [leg bat]; it depends
**    on fan through the tests made below, which u3z_ut_*() tracks.
*/
u3_noun
u3wfu_rest(u3_noun cor)
{
  u3_noun bat, leg, van, fan;

  if (  (c3n == u3r_mean(cor, {u3x_sam, &leg}, {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (fan = u3r_at(u3qfu_van_fan, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    c3_m    fun_m = 141 + c3__rest;
    u3_noun key   = u3z_key_2(fun_m, leg, bat);
    u3_weak pro   = u3z_ut_find(key, fan, cor);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      u3z_ut_frame fra_u;

      u3z_ut_enter(&fra_u, fan, u3z_ut_rest);
      pro = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
      u3z_ut_leave(&fra_u, key, pro);
      return pro;
    }
  }
}
