/// @file

#include "direct.h"

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "jets/q.h"
#include "log.h"
#include "manage.h"
#include "nock-compile.h"
#include "nock.h"
#include "retrieve.h"
#include "serial.h"
#include "trace.h"
#include "vortex.h"
#include "xtract.h"

#define _CVX_LOAD  4
#define _CVX_PEEK 22
#define _CVX_POKE 23
#define _CVX_WISH 10

/*  The SKA core is an arvo-shaped core whose +poke is the arm at axis 23:
**
**    [%full sub=* fol=^]                 ->  [%ir bell straight new]
**    [%dire bell mono=?]                 ->  [%ir bell straight ~]
**    [%jets (list [ring need-ordered])]  ->  [%drop (list bell)]
**    [%fast reset=? regs]                ->  ~
**
**  %drop: the functions a change of the jets invalidated, which the core
**  has forgotten the compiled forms of, and vere drops the programs of:
**  a driver added, changed or removed, or a ring for a function the core
**  had seen without one.  Those only change here: a core registered at
**  runtime gives rings to new functions alone, as programs may be running.
**  Its +peek at /ver is its version, and its +load is a gate from a core
**  of any version to itself with that core's state.
**
**  regs: (list [path sock]), cores registered with %fast hints, a label
**  path and the template of the core's shape.  jets.c builds the same for
**  the cores u3n registers, and keeps them all in u3R->ska.tem_p; those
**  the core hasn't seen wait in u3R->ska.new and are fed to it before it
**  analyzes.  new: (list [path sock clue]), the registrations the analysis
**  made, parents first, with the clues of their hints: u3n registers the
**  cores from those, as compiled nock runs no %fast hint.
*/

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
#ifdef U3NC_STAT
  u3nc_Stat.hug_d++;
  if ( (c3y == u3r_sing(cape_one, cape_two)) && (data_one != data_two) ) {
    c3_o ret_o;
#  ifdef __x86_64__
    c3_d tsc_d = __builtin_ia32_rdtsc();
#  endif
    u3nc_Stat.hus_d++;
    ret_o = u3r_sing(data_one, data_two);
#  ifdef __x86_64__
    u3nc_Stat.hus_c += __builtin_ia32_rdtsc() - tsc_d;
#  endif
    if ( c3y == ret_o ) {
      return c3y;
    }
  }
  else
#endif
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
    //  XX manual stack?
    //
    //  holding uncounted references for one side while comparing nouns
    //  in the other should be fine because the fact that both sides make a cell
    //  is reflected in the refcounts
    return c3a(_d_huge(lope, l_one, loop, l_two),
               _d_huge(rope, r_one, roop, r_two));
  }
}

/* _d_huge_las(): does sub nest under the sock [cape data], given las, a
**   noun that nested under it before, or u3_none?  A part of sub that is
**   the very same noun as the part of las at its axis nests too, so it is
**   neither walked nor compared: only the parts that changed since las
**   are checked, and an equal copy is compared once, when it first comes.
**   RETAINS.
*/
static c3_o
_d_huge_las(u3_noun cape, u3_noun data, u3_noun sub, u3_weak las)
{
  while ( 1 ) {
    if ( sub == las ) {
      return c3y;
    }
#ifdef U3NC_STAT
    u3nc_Stat.hug_d++;
#endif
    if ( c3y == u3ud(cape) ) {
      c3_o ret_o;

      if ( c3n == _d_loob(cape) ) {
        return c3y;
      }
#ifdef U3NC_STAT
      if ( data != sub ) {
#  ifdef __x86_64__
        c3_d tsc_d = __builtin_ia32_rdtsc();
#  endif
        u3nc_Stat.hus_d++;
        ret_o = u3r_sing(data, sub);
#  ifdef __x86_64__
        u3nc_Stat.hus_c += __builtin_ia32_rdtsc() - tsc_d;
#  endif
        return ret_o;
      }
#endif
      ret_o = u3r_sing(data, sub);
      return ret_o;
    }

    if ( c3n == u3du(sub) ) {
      return c3n;
    }

    {
      u3_noun dh, dt;
      u3_weak lh = u3_none, lt = u3_none;

      u3x_cell(data, &dh, &dt);
      if ( (u3_none != las) && (c3y == u3du(las)) ) {
        lh = u3h(las);
        lt = u3t(las);
      }
      if ( c3n == _d_huge_las(u3h(cape), dh, u3h(sub), lh) ) {
        return c3n;
      }
      cape = u3t(cape);
      data = dt;
      sub  = u3t(sub);
      las  = lt;
    }
  }
}

/* u3d_match_las(): u3d_match(), checking each candidate i against the
**   last subject that matched it, las_f(i, ptr_v) (transferred, or
**   u3_none).  RETAINS.
*/
u3_weak
u3d_match_las(u3_noun sub, u3_noun lis,
              u3_weak (*las_f)(u3_noun, void*), void* ptr_v)
{
  u3_weak pro = u3_none;
  u3_noun cape_max = 0, data_max = 0;
  u3_noun i, cape_i, data_i;

  while ( u3_nul != lis ) {
    u3_weak las;
    c3_o    hug_o;

    u3x_cell(lis, &i, &lis);
    cape_i = u3h(u3h(i));
    data_i = u3t(u3h(i));
    las    = las_f(i, ptr_v);
    hug_o  = _d_huge_las(cape_i, data_i, sub, las);

    if ( u3_none != las ) {
      u3z(las);
    }
    if ( c3n == hug_o ) {
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
    cape_i = u3h(u3h(i));
    data_i = u3t(u3h(i));

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

/* _d_slog(): print a list of tanks.  RETAINS.
*/
static void
_d_slog(u3_noun tan)
{
  for ( ; u3_nul != tan; tan = u3t(tan) ) {
    u3t_slog(u3nc(0, u3k(u3h(tan))));
  }
}

/* _d_ver_d: commits of a core state that changes what the core knows: of
**           every poke but %dire, whose changes are caches.  A poke made
**           from inside another one (the core running its own code
**           compiled, see u3a_flag_ska) commits into the junior road of the
**           outer poke, whose product, computed from the state before,
**           would undo it on return: the outer poke then runs again, on the
**           new state.
*/
static c3_d _d_ver_d;

/* _d_poke(): poke the SKA core, rebails.
*/
static u3_noun
_d_poke(u3_noun ovo)
{
  u3_noun gat, pro, res;

  u3_assert( u3_nul != u3R->ska.cor );

  c3_m    tag_m = u3h(ovo);
  c3_d    beg_d = u3t_trace_time();
  c3_c    tag_c[5] = {0};
  c3_w    try_w = 0;

  memcpy(tag_c, &tag_m, 4);
  if ( getenv("U3NC_VERBOSE") ) {
    u3l_log("ska: poke %%%s: start, ovum mug %08x", tag_c, u3r_mug(u3t(ovo)));
  }

  while ( 1 ) {
    c3_d ver_d = _d_ver_d;
    c3_w fag_w = u3R->how.fag_w;
    c3_w dep_w = u3a_ska_depth(fag_w);

    //  the core runs one poke deeper, see u3a_flag_ska
    //
    u3R->how.fag_w = (fag_w & ~u3a_flag_ska)
                   | ((( dep_w < 3 ) ? (dep_w + 1) : 3) << 4);
    gat = u3n_nock_on(u3k(u3R->ska.cor), u3nt(9, _CVX_POKE, u3nc(0, 1)));
    pro = u3n_slam_et(u3_nul, gat, u3k(ovo));
    u3R->how.fag_w = (u3R->how.fag_w & ~u3a_flag_ska)
                   | (fag_w & u3a_flag_ska);

    if ( 0 != u3h(pro) ) {
      u3l_log("ska: poke failed: %%%" PRIc3_w ", %" PRIc3_w " frames", u3h(pro),
              ( 2 == u3h(pro) ) ? u3qb_lent(u3t(pro)) : 0);

      //  the trace of a crash, rendered
      //
      if ( 2 == u3h(pro) ) {
        u3_noun tan = u3t_mook(u3k(u3t(pro)));

        _d_slog(tan);
        u3z(tan);
      }

      u3z(pro);
      u3z(ovo);
      return u3m_bail(c3__fail);
    }

    if ( ver_d == _d_ver_d ) {
      break;
    }

    //  a poke made from inside this one changed the state it ran on
    //
    try_w++;
    if ( getenv("U3NC_VERBOSE") ) {
      u3l_log("ska: poke %%%s: the state changed underneath, again (%u)",
              tag_c, try_w);
    }
    u3z(pro);
  }

  u3z(ovo);
  res = u3k(u3h(u3t(pro)));

  if ( getenv("U3NC_VERBOSE") ) {
    u3l_log("ska: poke %%%s: %" PRIu64 " us", tag_c,
            u3t_trace_time() - beg_d);
  }

  u3z(u3R->ska.cor);
  u3R->ska.cor = u3k(u3t(u3t(pro)));
  u3z(pro);

  if ( c3__dire != tag_m ) {
    _d_ver_d++;
  }

  return res;
}

/* _d_fast(): feed registrations to the core, in place of what it has if
**            res_o.  TRANSFERS lis.
*/
static void
_d_fast(c3_o res_o, u3_noun lis)
{
  u3z(_d_poke(u3nt(c3__fast, res_o, lis)));
}

/* _d_flush(): feed the core the registrations it hasn't seen.
*/
static void
_d_flush(void)
{
  u3_noun lis = u3R->ska.new;

  if ( u3_nul != lis ) {
    u3R->ska.new = u3_nul;
    _d_fast(c3n, lis);
  }
}

/* _d_ir(): unpack [%ir bell straight new], registering the cores the
**          analysis registered: for SKA, by their [path sock]; for u3n,
**          by their clues through u3j_mine(), which locates the data of
**          a template as it would the core.
*/
static u3_noun
_d_ir(u3_noun pro, u3_noun* bell)
{
  u3_noun tag, bel, ir, new, pax, tem, clu;

  u3x_qual(pro, &tag, &bel, &ir, &new);

  if ( c3__ir != tag ) {
    u3m_bail(c3__fail);
  }

  if ( bell ) {
    *bell = u3k(bel);
  }

  for ( ; u3_nul != new; new = u3t(new) ) {
    u3x_trel(u3h(new), &pax, &tem, &clu);
    u3j_ska_put(u3nc(u3k(pax), u3k(tem)), c3n);
    u3j_mine(u3k(clu), u3k(u3t(tem)));
  }

  u3k(ir);
  u3z(pro);
  return ir;
}

/* _d_shape(): $need-ordered from flat axis array, assumes the array is depth-
**             first-ordered. Consumes the array. Assumes that the shape never
**             includes %both case.
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

/* _d_rings_cb(): collect [ring need-ordered] of one jet arm that has an
**                array driver and is on.
*/
static void
_d_rings_cb(u3_noun kev, void* ptr_v)
{
  u3_noun*        lis = ptr_v;
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;

  u3j_ring(u3h(kev), &ham_u, &arm_u);

  if ( arm_u ) {
    //  XX VLA. Assumes sane number of arguments in the driver
    c3_l axe_l[arm_u->len_w];

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

/* _d_version(): the version of a core.  RETAINS.
*/
static u3_noun
_d_version(u3_noun cor)
{
  u3_noun gat = u3n_nock_on(u3k(cor), u3nt(9, _CVX_PEEK, u3nc(0, 1)));
  u3_noun pro = u3n_slam_on(gat, u3nc(c3__ver, u3_nul));
  u3_noun ver = u3k(u3t(u3t(pro)));

  u3z(pro);
  return ver;
}

/* _d_load(): the core of a jammed steel pill [%steel nok trap ~], if none is
**            installed; if one is, ~ if it is of the pill's version, else the
**            pill's +load of it.  All on the junior road: the pill is cued
**            here and hash-consed, so that the kernel of the core is the
**            one the cold state has batteries of, for jet lookups not to
**            compare them whole, and a pill that isn't kept costs the home
**            road nothing.  RETAINS.
*/
static u3_noun
_d_load(u3_atom pil)
{
  u3_noun old   = u3R->ska.cor;
  u3_noun steel = u3m_dedup(u3s_cue_atom(pil));
  u3_noun tag, nok, trap, nul, cor, ver, ole;

  u3x_qual(steel, &tag, &nok, &trap, &nul);

  if ( (c3n == u3r_sing_c("steel", tag)) || (u3_nul != nul) ) {
    u3m_bail(c3__fail);
  }

  cor = u3n_nock_on(u3nc(u3k(trap), u3_nul), u3k(nok));
  u3z(steel);

  if ( u3_nul == old ) {
    return cor;
  }

  ver = _d_version(cor);
  ole = _d_version(old);

  if ( c3y == u3r_sing(ver, ole) ) {
    u3z(cor);
    return u3_nul;
  }

  u3l_log("ska: loading the core of version %" PRIc3_w " into version %"
          PRIc3_w, ole, ver);

  return u3n_slam_on(u3n_nock_on(cor, u3nt(9, _CVX_LOAD, u3nc(0, 1))),
                     u3k(old));
}

/* u3d_boot(): bring the SKA core up to a jammed steel pill: install it if
**             there is no core; if the pill's version is not the installed
**             core's, replace the core by the pill's +load of it and drop
**             all programs; then u3d_hot().  The empty pill, (jam 0), is
**             a binary without SKA: no core, and one a previous binary
**             installed goes, with its programs.  TRANSFERS.
*/
void
u3d_boot(u3_atom pil)
{
  c3_o    nuu_o;
  u3_noun pro;

  if ( 2 == pil ) {
    if ( u3_nul != u3R->ska.cor ) {
      u3l_log("ska: no core in this binary, dropping the installed one");
      u3z(u3R->ska.cor);
      u3R->ska.cor = u3_nul;
      u3z(u3R->ska.new);
      u3R->ska.new = u3_nul;
      u3nc_reclaim();
    }
    return;
  }

  nuu_o = __(u3_nul == u3R->ska.cor);
  pro   = u3m_soft(0, _d_load, pil);

  if ( 0 != u3h(pro) ) {
    u3l_log("ska: boot failed: %%%s", u3r_string(u3h(pro)));
    _d_slog(u3t(pro));
    u3m_bail(c3__fail);
  }

  if ( u3_nul != u3t(pro) ) {
    u3z(u3R->ska.cor);
    u3R->ska.cor = u3k(u3t(pro));

    if ( c3n == nuu_o ) {
      u3nc_reclaim();
    }
  }

  u3z(pro);

  u3d_hot();

  if ( c3y == nuu_o ) {
    u3d_cold();
  }
}

/* _d_jets(): feed the core the jet drivers lis, and drop the programs of
**            the functions a change in them invalidates.  TRANSFERS.
*/
static void
_d_jets(u3_noun lis)
{
  u3_noun pro, tag, bes;

  u3l_log("ska: %" PRIc3_w " jetted arms", u3qb_lent(lis));

  _d_flush();
  pro = _d_poke(u3nc(c3__jets, lis));
  u3x_cell(pro, &tag, &bes);

  if ( c3__drop != tag ) {
    u3m_bail(c3__fail);
  }

  if ( u3_nul != bes ) {
    u3l_log("ska: the jets changed: dropping %" PRIc3_w " functions",
            u3qb_lent(bes));
    u3nc_drop(bes);
  }

  u3z(pro);
}

/* _d_skip(): the drivers of lis but those through the rings in ban.
**            RETAINS.
*/
static u3_noun
_d_skip(u3_noun lis, u3_noun ban)
{
  u3_noun out = u3_nul, i, j;

  for ( ; u3_nul != lis; lis = u3t(lis) ) {
    i = u3h(lis);

    for ( j = ban; u3_nul != j; j = u3t(j) ) {
      if ( c3y == u3r_sing(u3h(i), u3h(j)) ) {
        break;
      }
    }

    if ( u3_nul == j ) {
      out = u3nc(u3k(i), out);
    }
  }

  return out;
}

/* u3d_hot(): feed the core the jet drivers there are, and drop the programs
**            of the functions a change in them invalidates.  The trust of
**            an arm (its ice flag) is no concern of the core's, which the
**            driver is all the same to, but a compiled call through it
**            either checks the driver or doesn't: to drop the callers of
**            an arm whose trust changed since they compiled (u3nc_stale()),
**            the core is fed the drivers without it first, as if it were
**            gone, then with it.
*/
void
u3d_hot(void)
{
  u3_noun lis = _d_rings();
  u3_noun bad = u3nc_stale();

  if ( u3_nul != bad ) {
    u3l_log("ska: the trust of %" PRIc3_w " jets changed", u3qb_lent(bad));
    _d_jets(_d_skip(lis, bad));
  }

  u3z(bad);
  _d_jets(lis);
}

/* _d_tem_cb(): collect the registrations of a battery.
*/
static void
_d_tem_cb(u3_noun kev, void* ptr_v)
{
  u3_noun* lis = ptr_v;
  u3_noun  i;

  for ( i = u3t(kev); u3_nul != i; i = u3t(i) ) {
    *lis = u3nc(u3k(u3h(i)), *lis);
  }
}

/* u3d_cold(): feed the core every registration there is, in place of
**             those it has: after installing a core.
*/
void
u3d_cold(void)
{
  u3a_road* rod_u;
  u3_noun   lis = u3_nul;

  for ( rod_u = u3R; rod_u; rod_u = u3tn(u3a_road, rod_u->par_p) ) {
    u3h_walk_with(rod_u->ska.tem_p, _d_tem_cb, &lis);
  }

  u3l_log("ska: %" PRIc3_w " registered cores", u3qb_lent(lis));
  u3z(u3R->ska.new);
  u3R->ska.new = u3_nul;
  _d_fast(c3y, lis);
}

/* u3d_full(): compile [sub fol] as a unary function of its whole
**             subject.  RETAINS sub and fol.  Produces the IR and,
**             in *bell, the [sock formula] identity of the function.
*/
u3_noun
u3d_full(u3_noun sub, u3_noun fol, u3_noun* bell)
{
  _d_flush();
  return _d_ir(_d_poke(u3nt(c3__full, u3k(sub), u3k(fol))), bell);
}

/* u3d_dire(): compile a bell [sock formula] as a function of the
**             parts of its subject that it uses.  RETAINS. Assumes that the
**             analysis of its transitive caller was already done (how else
**             would we get the bell?)
*/
u3_noun
u3d_dire(u3_noun bell, c3_o mon_o)
{
  _d_flush();
  return _d_ir(_d_poke(u3nt(c3__dire, u3k(bell), mon_o)), 0);
}
