/// @file

#include "direct.h"

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "jets/k.h"
#include "jets/q.h"
#include "log.h"
#include "manage.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
#include "xtract.h"

/*  The SKA core is an arvo-shaped core whose +poke is the arm at axis 23:
**
**    [%full sub=* fol=^]                 ->  [%ir bell straight]
**    [%dire bell]                        ->  [%ir bell straight]
**    [%jets (list [ring need-ordered])]  ->  ~
*/
#define _d_poke_w  23

/* _d_loob(): assert loobean.
*/
static inline c3_o
_d_loob(u3_noun som)
{
  if ( som > 1 ) {
    u3m_bail(c3__fail);
  }
  return som;
}

/* _d_rip(): head and tail of a cape.  RETAINS.
*/
static void
_d_rip(u3_noun cape, u3_noun* l, u3_noun* r)
{
  if ( c3y == u3ud(cape) ) {
    *l = *r = _d_loob(cape);
  }
  else {
    u3x_cell(cape, l, r);
  }
}

/* _d_huge(): does sock two nest under sock one?  RETAINS.
**            (everything known by one is also known by two)
*/
static c3_o
_d_huge(u3_noun cape_one, u3_noun data_one,
        u3_noun cape_two, u3_noun data_two)
{
  if (  (c3y == u3r_sing(cape_one, cape_two))
     && (c3y == u3r_sing(data_one, data_two)) )
  {
    return c3y;
  }

  if ( c3y == u3ud(data_one) ) {
    if ( c3n == _d_loob(cape_one) ) {
      return c3y;
    }
    return c3a(u3ud(cape_two),
           c3a(_d_loob(cape_two),
               u3r_sing(data_one, data_two)));
  }

  u3_assert( c3n != cape_one );

  if ( c3y == u3ud(data_two) ) {
    _d_loob(cape_two);
    return c3n;
  }

  {
    u3_noun lope, rope, loop, roop;
    u3_noun l_one, r_one, l_two, r_two;

    u3x_cell(data_one, &l_one, &r_one);
    u3x_cell(data_two, &l_two, &r_two);
    _d_rip(cape_one, &lope, &rope);
    _d_rip(cape_two, &loop, &roop);

    return c3a(_d_huge(lope, l_one, loop, l_two),
               _d_huge(rope, r_one, roop, r_two));
  }
}

/* u3d_match(): find the most specific [sock *] in `lis` whose sock
**              matches sub.  RETAINS.  Produces u3_none if none do.
*/
u3_weak
u3d_match(u3_noun sub, u3_noun lis)
{
  u3_weak pro = u3_none;
  u3_noun cape_max = 0, data_max = 0;
  u3_noun i, cape_i, data_i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    u3x_mean(i, {4, &cape_i}, {5, &data_i});

    if ( c3n == _d_huge(cape_i, data_i, c3y, sub) ) {
      continue;
    }

    if (  (u3_none == pro)
       || (c3y == _d_huge(cape_max, data_max, cape_i, data_i)) )
    {
      pro      = i;
      cape_max = cape_i;
      data_max = data_i;
    }
  }

  return pro;
}

/* _d_poke(): poke the SKA core.  TRANSFERS ovo, produces the product,
**            and replaces the core.
*/
static u3_noun
_d_poke(u3_noun ovo)
{
  u3_noun cor = u3R->ska.cor;
  u3_noun gat, pro, res;

  u3_assert( u3_nul != cor );

  gat = u3n_nock_on(u3k(cor), u3nt(9, _d_poke_w, u3nc(0, 1)));
  pro = u3n_slam_on(gat, ovo);
  res = u3k(u3h(pro));

  u3z(u3R->ska.cor);
  u3R->ska.cor = u3k(u3t(pro));
  u3z(pro);

  return res;
}

/* _d_ir(): unpack [%ir bell straight].  TRANSFERS; produces the
**          straight and, if asked, the bell.
*/
static u3_noun
_d_ir(u3_noun pro, u3_noun* bell)
{
  u3_noun tag, bel, ir;

  u3x_trel(pro, &tag, &bel, &ir);

  if ( c3__ir != tag ) {
    u3m_bail(c3__fail);
  }

  if ( bell ) {
    *bell = u3k(bel);
  }

  ir = u3k(ir);
  u3z(pro);
  return ir;
}

/* _d_shape(): need-ordered shape of the arguments at axes `axe_l`,
**             sorted by a preorder traversal.  Consumes the axes.
*/
static u3_noun
_d_shape(c3_l* axe_l, c3_w len_w)
{
  c3_w piv_w = 0, i_w;

  if ( 0 == len_w ) {
    return u3nc(c3__none, u3_nul);
  }
  if ( (1 == len_w) && (1 == axe_l[0]) ) {
    return u3nc(c3__this, u3_nul);
  }

  while ( (piv_w < len_w) && (2 == u3x_cap(axe_l[piv_w])) ) {
    axe_l[piv_w] = u3x_mas(axe_l[piv_w]);
    piv_w++;
  }
  for ( i_w = piv_w; i_w < len_w; i_w++ ) {
    axe_l[i_w] = u3x_mas(axe_l[i_w]);
  }

  return u3nc(_d_shape(axe_l, piv_w),
              _d_shape(axe_l + piv_w, len_w - piv_w));
}

/* _d_rings_cb(): collect [ring need-ordered] of one jet arm.
*/
static void
_d_rings_cb(u3_noun kev, void* ptr_v)
{
  u3_noun* lis = ptr_v;
  u3_noun  jax, inx, arg;

  u3x_trel(u3t(kev), &jax, &inx, &arg);

  if ( arg ) {
    const u3u_harm* arm_u = &(u3u_Harm[arg - 1]);
    c3_l            axe_l[arm_u->len_w];

    memcpy(axe_l, arm_u->axe_l, sizeof(axe_l));
    *lis = u3nc(u3nc(u3k(u3h(kev)), _d_shape(axe_l, arm_u->len_w)), *lis);
  }
}

/* _d_rings(): (list [ring need-ordered]) of all array-jetted arms.
*/
static u3_noun
_d_rings(void)
{
  u3_noun lis = u3_nul;
  u3h_walk_with(u3H->rod_u.ska.pax_p, _d_rings_cb, &lis);
  return lis;
}

/* u3d_boot(): install the SKA core from a steel pill [%steel nok trap ~].
*/
void
u3d_boot(u3_noun steel)
{
  u3_noun tag, nok, trap, nul, cor;

  u3x_qual(steel, &tag, &nok, &trap, &nul);

  if ( (c3n == u3r_sing_c("steel", tag)) || (u3_nul != nul) ) {
    u3m_bail(c3__fail);
  }

  cor = u3n_nock_on(u3nc(u3k(trap), u3_nul), u3k(nok));
  u3z(steel);

  u3z(u3R->ska.cor);
  u3R->ska.cor = cor;

  {
    u3_noun lis = _d_rings();
    u3l_log("ska: %u jetted arms", u3qb_lent(lis));
    u3z(_d_poke(u3nc(c3__jets, lis)));
  }
}

/* u3d_full(): compile [sub fol] as a unary function of its whole
**             subject.  RETAINS sub and fol.  Produces the IR and,
**             in *bell, the [sock formula] identity of the function.
*/
u3_noun
u3d_full(u3_noun sub, u3_noun fol, u3_noun* bell)
{
  return _d_ir(_d_poke(u3nt(c3__full, u3k(sub), u3k(fol))), bell);
}

/* u3d_dire(): compile a bell [sock formula] as a function of the
**             parts of its subject that it uses.  RETAINS.
*/
u3_noun
u3d_dire(u3_noun bell)
{
  return _d_ir(_d_poke(u3nc(c3__dire, u3k(bell))), 0);
}
