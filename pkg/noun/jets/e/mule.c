/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "nock-compile.h"
#include "noun.h"

/* _mule(): slam the +mute hook on the sample through slam_f.
*/
static u3_noun
_mule(u3_noun cor, u3_noun (*slam_f)(u3_noun, u3_noun))
{
  u3_noun hok = u3j_cook("u3we_mule-mute", u3k(cor), "mute");


  //  this takes advantage of the fact that +mute's result is
  //  identical to that of +mule, and safely produces a statically-typed
  //  value while only evaluating the trap once.
  //
  return slam_f(hok, u3k(u3x_at(u3x_sam, cor)));
}

/* u3we_mule(): from u3n, in u3n.
*/
u3_noun
u3we_mule(u3_noun cor)
{
  return _mule(cor, u3n_slam_on);
}

/* u3ue_mule(): from compiled nock, in compiled nock.
*/
u3_noun
u3ue_mule(u3_noun cor)
{
  return _mule(cor, u3nc_slam_on);
}
