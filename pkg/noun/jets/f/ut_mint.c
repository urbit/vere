/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3wfu_mint(u3_noun cor)
{
  u3_noun fan, bat, sut, gol, gen, van;

  if (  (c3n == u3r_mean(cor, {u3x_sam_2, &gol},
                              {u3x_sam_3, &gen},
                              {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (fan = u3r_at(u3qfu_van_fan, van)))
     || (u3_none == (sut = u3r_at(u3x_sam, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    c3_m  fun_m = 141 + c3__mint;
    u3_weak vet = u3r_at(u3qfu_van_vet, van);
    if ( u3_none == vet ) return u3m_bail(c3__fail);
    u3_noun key = u3z_key_5(fun_m, vet, sut, gol, gen, bat);
    u3_weak pro = u3z_ut_find(key, fan, cor);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      u3z_ut_frame fra_u;

      u3z_ut_enter(&fra_u, fan, u3z_ut_top);
      pro = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
      u3z_ut_leave(&fra_u, key, pro);
      return pro;
    }
  }
}
