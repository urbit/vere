/// @file

#include "vortex.h"

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets/k.h"
#include "jets/q.h"
#include "log.h"
#include "manage.h"
#include "nock.h"
#include "retrieve.h"
#include "trace.h"
#include "xtract.h"

#define _CVX_LOAD  4
#define _CVX_PEEK 22
#define _CVX_POKE 23
#define _CVX_WISH 10

u3v_home* u3v_Home;

/* u3v_life(): execute initial lifecycle, producing Arvo core.
*/
u3_noun
u3v_life(u3_noun eve)
{
  u3_noun lyf = u3nt(2, u3nc(0, 3), u3nc(0, 2));
  u3_noun gat = u3n_nock_on(eve, lyf);
  u3_noun cor = u3k(u3x_at(7, gat));

  u3z(gat);
  return cor;
}

/* u3v_boot(): evaluate boot sequence, making a kernel
*/
c3_o
u3v_boot(u3_noun eve)
{
  c3_d len_d;
  {
    u3_noun len = u3qb_lent(eve);
    u3_assert( c3y == u3r_safe_chub(len, &len_d) );
    u3z(len);
  }

  {
    u3_noun pro = u3m_soft(0, u3v_life, eve);

    if ( u3_blip != u3h(pro) ) {
      u3_noun mot = u3h(pro);
      if ( _(u3a_is_cat(mot)) ) {
        c3_c mot_c[5] = {0};
        mot_c[0] = (mot >>  0) & 0xff;
        mot_c[1] = (mot >>  8) & 0xff;
        mot_c[2] = (mot >> 16) & 0xff;
        mot_c[3] = (mot >> 24) & 0xff;
        fprintf(stderr, "boot: bail: %%%s\r\n", mot_c);
      }
      else {
        fprintf(stderr, "boot: bail\r\n");
      }
      u3z(pro);
      return c3n;
    }

    u3z(u3A->roc);
    u3A->roc   = u3k(u3t(pro));
    u3A->eve_d = len_d;
    u3z(pro);
  }

  return c3y;
}

/* _cv_lite(): load lightweight, core-only pill.
*/
static u3_noun
_cv_lite(u3_noun pil)
{
  u3_noun eve, pro;

  {
    u3_noun hed, tal;
    u3x_cell(pil, &hed, &tal);
    if ( !_(u3r_sing_c("ivory", hed)) ) {
      u3m_bail(c3__exit);
    }
    eve = tal;
  }

  u3l_log("lite: arvo formula %x", u3r_mug(pil));
  pro = u3v_life(u3k(eve));
  u3l_log("lite: core %x", u3r_mug(pro));

  u3z(pil);
  return pro;
}

/* u3v_boot_lite(): light bootstrap sequence, just making a kernel.
*/
c3_o
u3v_boot_lite(u3_noun pil)
{
  //  ensure zero-initialized kernel
  //
  u3A->roc = 0;

  {
    u3_noun pro = u3m_soft(0, _cv_lite, pil);

    if ( u3_blip != u3h(pro) ) {
      u3z(pro);
      return c3n;
    }

    u3A->roc = u3k(u3t(pro));
    u3z(pro);
  }

  u3l_log("lite: final state %x", u3r_mug(u3A->roc));

  return c3y;
}

/* _cv_nock_wish(): call wish through hardcoded interface.
*/
static u3_noun
_cv_nock_wish(u3_noun txt)
{
  u3_noun fun, pro;

  fun = u3n_nock_on(u3k(u3A->roc), u3k(u3x_at(_CVX_WISH, u3A->roc)));
  pro = u3n_slam_on(fun, txt);

  return pro;
}

/* u3v_wish_w(): text expression with cache. with the input as a u3_noun.
*/
u3_noun
u3v_wish_w(u3_noun txt)
{
  u3t_event_trace("u3v_wish", 'b');
  u3_weak exp = u3kdb_get(u3k(u3A->yot), u3k(txt));

  if ( u3_none == exp ) {
    exp = _cv_nock_wish(u3k(txt));

    //  It's probably not a good idea to use u3v_wish()
    //  outside the top level... (as the result is uncached)
    //
    if ( u3R == &u3H->rod_u ) {
      u3A->yot = u3kdb_put(u3A->yot, u3k(txt), u3k(exp));
    }
  }

  u3t_event_trace("u3v_wish", 'e');

  u3z(txt);
  return exp;
}

/* u3v_wish(): text expression with cache.
*/
u3_noun
u3v_wish(const c3_c* str_c)
{
  u3t_event_trace("u3v_wish", 'b');
  u3_noun txt = u3i_string(str_c);
  u3_weak exp = u3kdb_get(u3k(u3A->yot), u3k(txt));

  if ( u3_none == exp ) {
    exp = _cv_nock_wish(u3k(txt));

    //  It's probably not a good idea to use u3v_wish()
    //  outside the top level... (as the result is uncached)
    //
    if ( u3R == &u3H->rod_u ) {
      u3A->yot = u3kdb_put(u3A->yot, u3k(txt), u3k(exp));
    }
  }

  u3t_event_trace("u3v_wish", 'e');

  u3z(txt);
  return exp;
}

/* u3v_do(): use a kernel gate.
*/
u3_noun
u3v_do(const c3_c* txt_c, u3_noun sam)
{
  u3_noun gat = u3v_wish(txt_c);
  u3_noun pro;

#if 0
  if ( &u3H->rod_u == u3R ) {
    pro = u3m_soft_slam(gat, sam);
  }
  else {
    pro = u3n_slam_on(gat, sam);
  }
#else
  pro = u3n_slam_on(gat, sam);
#endif

  return pro;
}

/* u3v_lily(): parse little atom.
*/
c3_o
u3v_lily(u3_noun fot, u3_noun txt, c3_l* tid_l)
{
  c3_w wad_w;
  u3_noun uco = u3dc("slaw", fot, u3k(txt));
  u3_noun p_uco, q_uco;

  if ( (c3n == u3r_cell(uco, &p_uco, &q_uco)) ||
       (u3_nul != p_uco) ||
       (c3n == u3r_safe_word(q_uco, &wad_w)) ||
       (wad_w & u3a_indirect_flag) )
  {
    c3_c* txt_c = u3r_string(txt);
    u3l_log("strange lily %s", txt_c);
    c3_free(txt_c);
    u3z(txt); u3z(uco);
    return c3n;
  }
  else {
    *tid_l = (c3_l)wad_w;
    u3z(txt); u3z(uco);
    return c3y;
  }
}

/* u3v_peek(): query the reck namespace (protected).
*/
u3_noun
u3v_peek(u3_noun sam)
{
  u3_noun fun = u3n_nock_on(u3k(u3A->roc), u3k(u3x_at(_CVX_PEEK, u3A->roc)));
  return u3n_slam_on(fun, sam);
}

/* u3v_soft_peek(): softly query the reck namespace.
*/
u3_noun
u3v_soft_peek(c3_w mil_w, u3_noun sam)
{
  u3_noun gon = u3m_soft(mil_w, u3v_peek, sam);
  u3_noun tag, dat;
  u3x_cell(gon, &tag, &dat);

  //  read failed, produce trace
  //
  //    NB, reads *should not* fail deterministically
  //
  if ( u3_blip != tag ) {
    return u3nc(c3n, gon);
  }

  //  read succeeded, produce result
  //
  {
    u3_noun pro = u3nc(c3y, u3k(dat));
    u3z(gon);
    return pro;
  }
}

/* u3v_poke(): compute a timestamped ovum.
*/
u3_noun
u3v_poke(u3_noun sam)
{
  u3_noun fun = u3n_nock_on(u3k(u3A->roc), u3k(u3x_at(_CVX_POKE, u3A->roc)));
  u3_noun pro;

  {
# ifdef  U3_MEMORY_DEBUG
    c3_w cod_w = u3a_lush(u3h(u3t(u3t(sam))));
# endif

    pro = u3n_slam_on(fun, sam);

# ifdef  U3_MEMORY_DEBUG
    u3a_lop(cod_w);
# endif
  }

  return pro;
}

/* u3v_poke_sure(): inject an event, saving new state if successful.
*/
c3_o
u3v_poke_sure(c3_w mil_w, u3_noun eve, u3_noun* pro)
{
  u3_noun gon = u3m_soft(mil_w, u3v_poke, eve);
  u3_noun tag, dat;
  u3x_cell(gon, &tag, &dat);

  //  event failed, produce trace
  //
  if ( u3_blip != tag ) {
    *pro = gon;
    return c3n;
  }

  //  event succeeded, persist state and produce effects
  //
  {
    u3_noun vir, cor;
    u3x_cell(dat, &vir, &cor);

    u3z(u3A->roc);
    u3A->roc = u3k(cor);
    u3A->eve_d++;

    *pro = u3k(vir);
    u3z(gon);
    return c3y;
  }
}

/* u3v_tank(): dump single tank.
*/
void
u3v_tank(u3_noun blu, c3_l tab_l, u3_noun tac)
{
  u3v_punt(blu, tab_l, u3nc(tac, u3_nul));
}

/* u3v_punt(): dump tank list.
*/
void
u3v_punt(u3_noun blu, c3_l tab_l, u3_noun tac)
{
#if 0
  u3_noun blu   = u3_term_get_blew(0);
#endif
  c3_l    col_l = u3h(blu);
  u3_noun cat   = tac;

  //  We are calling nock here, but hopefully need no protection.
  //
  while ( c3y == u3du(cat) ) {
    u3_noun wol = u3dc("wash", u3nc(tab_l, col_l), u3k(u3h(cat)));

    u3m_wall(wol);
    cat = u3t(cat);
  }
  u3z(tac);
  u3z(blu);
}

/* u3v_sway(): print trace.
*/
void
u3v_sway(u3_noun blu, c3_l tab_l, u3_noun tax)
{
  u3_noun mok = u3dc("mook", 2, tax);

  u3v_punt(blu, tab_l, u3k(u3t(mok)));
  u3z(mok);
}

/* u3v_mark(): mark arvo kernel.
*/
u3m_quac*
u3v_mark()
{
  u3v_arvo* arv_u = &(u3H->arv_u);

  u3m_quac** qua_u = c3_malloc(sizeof(*qua_u) * 3);

  qua_u[0] = c3_calloc(sizeof(*qua_u[0]));
  qua_u[0]->nam_c = strdup("kernel");
  qua_u[0]->siz_w = u3a_mark_noun(arv_u->roc) * sizeof(c3_w);

  qua_u[1] = c3_calloc(sizeof(*qua_u[1]));
  qua_u[1]->nam_c = strdup("wish cache");
  qua_u[1]->siz_w = u3a_mark_noun(arv_u->yot) * sizeof(c3_w);

  //  mark blob bank HAMT.  values are atoms encoding loom offsets of
  //  u3a_blob structs; the structs themselves are walloc'd blocks not
  //  subject to noun mark/sweep.  u3h_mark covers nodes, keys, values.
  //
  if ( u3H->blb_p ) {
    u3h_mark(u3H->blb_p);
  }

  qua_u[2] = NULL;

  u3m_quac* tot_u = c3_malloc(sizeof(*tot_u));
  tot_u->nam_c = strdup("total arvo stuff");
  tot_u->siz_w = qua_u[0]->siz_w + qua_u[1]->siz_w;
  tot_u->qua_u = qua_u;

  return tot_u;
}

/* u3v_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3v_reclaim(void)
{
  //  clear the u3v_wish cache
  //
  //    NB: this would leak if not on the home road
  //
  if ( &(u3H->rod_u) == u3R ) {
    u3z(u3A->yot);
    u3A->yot = u3_nul;
  }
}

/* _v_rewrite_blb_cb(): u3h_walk_with callback for compaction.
**
**   Each blb_p value atom encodes the loom offset of a u3a_blob struct.
**   pack_seek determines new offsets but the integer values stored
**   inside the value atoms are not noun pointers, so u3h_relocate
**   doesn't update them.  We mutate cell->tel in place to the new
**   offset.  Must run BEFORE u3h_relocate(&blb_p) — at that point
**   slot pointers and cells are still at their pre-pack addresses,
**   so u3a_to_ptr(kev) resolves correctly.
*/
static void
_v_rewrite_blb_cb(u3_noun kev, void* ptr_v)
{
  (void)ptr_v;
  u3a_cell* cel_u = (u3a_cell*)u3a_to_ptr(kev);

  //  tel is a direct atom; its value is the loom offset of the
  //  u3a_blob.  loom offsets always fit in cat range on both VERE32
  //  (≤30 bits) and VERE64 (≤34 bits).
  //
  u3_post off_p = (u3_post)cel_u->tel;
  u3a_relocate_post(&off_p);
  cel_u->tel = (u3_noun)off_p;
}

/* u3v_rewrite_compact(): rewrite arvo kernel for compaction.
*/
void
u3v_rewrite_compact(void)
{
  //  XX fix these to correctly no-op on inner roads
  //
  u3a_relocate_noun(&(u3A->roc));
  u3a_relocate_noun(&(u3A->yot));

  //  relocate blob bank HAMT.  The values are atoms encoding loom
  //  offsets of u3a_blob structs; rewrite those offsets BEFORE the
  //  HAMT structure is relocated so we can still read kev cells at
  //  their pre-pack addresses.
  //
  if ( u3H->blb_p ) {
    u3h_walk_with(u3H->blb_p, _v_rewrite_blb_cb, 0);
    u3h_relocate(&(u3H->blb_p));
  }
}
