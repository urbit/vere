/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  RETAINS
static u3_atom
_find_hoon_ver(u3_noun cor)
{
  while (c3y == u3du(cor)) {
    cor = u3t(cor);
  }
  u3_assert(c3y == u3a_is_cat(cor));
  return cor;
}

u3_noun
u3we_mule_pre_136(u3_noun cor)
{
  u3_noun gul = u3nt(
    //  battery
    //
    u3nq(
      u3nc(1, 0),                                             //  :-  ~
      u3nc(1, 0),                                             //  :-  ~
      2,                                                      //  .*
      u3nq(u3nc(0, 6),                                        //    +<
        1, 12, u3nt(u3nc(0, 2), 0, 3))                        //  [12 [0 2] 0 3]
    ),
    //  sample
    //
    u3nc(0, 0),
    //  context
    //
    0
  );

  u3_noun tone = u3n_nock_et_pre_136(gul,
    u3k(u3x_at(u3x_sam, cor)),
    u3nq(9, 2, 0, 1));
  
  u3_noun con = u3x_at(7, cor);
  u3_noun mook;
  switch (_find_hoon_ver(con)) {
    default: {
      u3l_log("hoon-version-lost");
      return u3m_bail(c3__fail);
    }
    case 140: {
      u3_noun fol = u3x_at(342, con);  //  XX is that right?
      mook = u3n_nock_on(u3k(con), u3k(fol));
    } break;
    
    case 139:
    case 138:
    case 137: {
      u3_noun fol = u3x_at(5621, con);
      mook = u3n_nock_on(u3k(con), u3k(fol));
    } break;
  }

  u3_noun toon = u3n_slam_on_pre_136(mook, tone);

  c3_dessert(c3y == u3du(toon));

  u3_noun pro;
  switch (u3h(toon)) {
    case 0: {
      pro = u3nc(c3y, u3k(u3t(toon)));
    } break;

    case 2: {
      pro = u3nc(c3n, u3k(u3t(toon)));
    } break;

    case 1: {
      u3_noun sof = u3do("(soft path)", u3k(u3t(toon)));
      u3_noun tank = (u3_nul == sof) ? u3nc(c3__leaf, u3i_tape("mute.hunk"))
                                     : u3do("smyt", u3k(u3t(sof)));
      pro = u3nt(c3n, tank, u3_nul);
      u3z(sof);
    } break;

    default: return u3m_bail(c3__fail);
  }
  u3z(toon);
  return pro;
}