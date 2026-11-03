/// @file

#include "retrieve.h"

#include "allocate.h"
#include "events.h"
#include "hashtable.h"
#include "imprison.h"
#include "murmur3.h"
#include "crib.h"
#include "trace.h"
#include "vortex.h"
#include "xtract.h"


//  declarations of inline functions
//
c3_o
u3r_cell(u3_noun a, u3_noun* b, u3_noun* c);
c3_o
u3r_trel(u3_noun a, u3_noun* b, u3_noun* c, u3_noun* d);
c3_o
u3r_qual(u3_noun  a,
         u3_noun* b,
         u3_noun* c,
         u3_noun* d,
         u3_noun* e);
c3_o
u3r_quil(u3_noun  a,
         u3_noun* b,
         u3_noun* c,
         u3_noun* d,
         u3_noun* e,
         u3_noun* f);
c3_o
u3r_hext(u3_noun  a,
         u3_noun* b,
         u3_noun* c,
         u3_noun* d,
         u3_noun* e,
         u3_noun* f,
         u3_noun* g);

/* u3r_cat: assert direct atom
*/
u3_atom u3r_cat(u3_noun a);

/* _frag_word(): fast fragment/branch prediction for top word.
*/
static u3_weak
_frag_word(c3_w a_w, u3_noun b)
{
  u3_assert(0 != a_w);

  {
    c3_w dep_w = u3x_dep(a_w);

    while ( dep_w ) {
      if ( c3n == u3a_is_cell(b) ) {
        return u3_none;
      }
      else {
        u3a_cell* b_u = u3a_to_ptr(b);

        b = *(((u3_noun*)&(b_u->hed)) + (1 & (a_w >> (dep_w - 1))));
        dep_w--;
      }
    }
    return b;
  }
}

/* _frag_deep(): fast fragment/branch for deep words.
*/
static u3_weak
_frag_deep(c3_w a_w, u3_noun b)
{
  c3_w dep_w = u3a_word_bits;

  while ( dep_w ) {
    if ( c3n == u3a_is_cell(b) ) {
      return u3_none;
    }
    else {
      u3a_cell* b_u = u3a_to_ptr(b);

      b = *(((u3_noun*)&(b_u->hed)) + (1 & (a_w >> (dep_w - 1))));
      dep_w--;
    }
  }
  return b;
}

/* u3r_at():
**
**   Return fragment (a) of (b), or u3_none if not applicable.
*/
u3_weak
u3r_at(u3_atom a, u3_noun b)
{
  u3_assert(u3_none != a);
  u3_assert(u3_none != b);

  u3t_on(far_o);

  if ( 0 == a ) {
    u3t_off(far_o);
    return u3_none;
  }

  if ( _(u3a_is_cat(a)) ) {
    u3t_off(far_o);
    return _frag_word(a, b);
  }
  else {
    if ( !_(u3a_is_pug(a)) ) {
      u3t_off(far_o);
      return u3_none;
    }
    else {
      u3a_atom* a_u = u3a_to_ptr(a);
      c3_w len_w      = a_u->len_w;

      b = _frag_word(a_u->buf_w[len_w - 1], b);
      len_w -= 1;

      if ( u3_none == b ) {
        u3t_off(far_o);
        return b;
      }

      while ( len_w ) {
        b = _frag_deep(a_u->buf_w[len_w - 1], b);

        if ( u3_none == b ) {
          u3t_off(far_o);

          return b;
        } else {
          len_w--;
        }
      }
      u3t_off(far_o);

      return b;
    }
  }
}

/* u3r_mean():
**
**   Attempt to deconstruct `a` by axis, noun pairs.
**   Axes must be sorted in tree order.
*/
  static c3_w
  _mean_cut(c3_w           len_w,
            u3r_mean_pair* prs_m)
  {
    c3_w i_w, cut_t, cut_w;

    cut_t = 0;
    cut_w = 0;
    for ( i_w = 0; i_w < len_w; i_w++ ) {
      c3_w axe_w = prs_m[i_w].axe_w;

      if ( (cut_t == 0) && (3 == u3x_cap(axe_w)) ) {
        cut_t = 1;
        cut_w = i_w;
      }
      prs_m[i_w].axe_w = u3x_mas(axe_w);
    }
    return cut_t ? cut_w : i_w;
  }

  static c3_o
  _mean_extract(u3_noun        som,
                c3_w           len_w,
                u3r_mean_pair* prs_m)
  {
    if ( len_w == 0 ) {
      return c3y;
    }
    else if ( (len_w == 1) && (1 == prs_m[0].axe_w) ) {
      *prs_m->som = som;
      return c3y;
    }
    else {
      if ( c3n == u3a_is_cell(som) ) {
        return c3n;
      } else {
        c3_w cut_w = _mean_cut(len_w, prs_m);

        return c3a
          (_mean_extract(u3a_h(som), cut_w, prs_m),
           _mean_extract(u3a_t(som), (len_w - cut_w), (prs_m + cut_w)));
      }
    }
  }

c3_o
u3r_vmean(u3_noun a, u3r_mean_pair pairs[], c3_z len_z)
{
  u3_assert(len_z < c3_w_max);
  return _mean_extract(a, (c3_w)len_z, pairs);
}

//  stack frame for tracking noun comparison and unification
//
//    we always compare arbitrary nouns in a none-frame.
//    when we compare two cells, we change the none-frame to a head-frame
//    and push a new none-frame for their heads. if the heads are equal,
//    we get the cells from the head-frame and unify their head pointers.
//    then, we convert the head-frame to a tail-frame and repeat with
//    the tails, mutatis mutandis.
//
//    in Hoon, this structure would be:
//
//    $%  [%none a=* b=*]
//        [%head a=^ b=^]
//        [%tail a=^ b=^]
//    ==
//
#define SING_NONE 0
#define SING_HEAD 1
#define SING_TAIL 2

typedef struct {
  c3_y     sat_y;
  u3_noun  a;
  u3_noun  b;
} eqframe;

/* _cr_sing_push(): push a new stack frame, initialized as SING_NONE.
*/
static inline eqframe*
_cr_sing_push(u3a_pile* pil_u, u3_noun a, u3_noun b)
{
  eqframe* fam_u = u3a_push(pil_u);
  fam_u->sat_y   = SING_NONE;
  fam_u->a       = a;
  fam_u->b       = b;
  return fam_u;
}

/* _cr_sing_mug(): short-circuit comparison if mugs are present and not equal.
*/
static inline c3_o
_cr_sing_mug(u3a_noun* a_u, u3a_noun* b_u)
{
  //  XX add debug assertions that both mugs are 31-bit
  //  (ie, not u3a_take() relocation references)
  //
  if ( a_u->mug_w && b_u->mug_w && (a_u->mug_w != b_u->mug_w) ) {
    return c3n;
  }

  return c3y;
}

/* _cr_sing_atom(): check if atom [a] is indirect and equal to noun [b]
*/
static inline c3_o
_cr_sing_atom(u3_atom a, u3_noun b)
{
  //  [a] is an atom, not pointer-equal to noun [b].
  //  if they're not both indirect atoms, they can't be equal.
  //
  if (  (c3n == u3a_is_pug(a))
     || (c3n == u3a_is_pug(b)) )
  {
    return c3n;
  }
  else {
    u3a_atom* a_u = u3a_to_ptr(a);
    u3a_atom* b_u = u3a_to_ptr(b);

    //  [a] and [b] are not equal if their mugs are present and not equal.
    //
    if ( c3n == _cr_sing_mug((u3a_noun*)a_u, (u3a_noun*)b_u) ) {
      return c3n;
    }
    else {
      c3_w a_w = a_u->len_w;
      c3_w b_w = b_u->len_w;

      //  [a] and [b] are not equal if their lengths are not equal
      //
      if ( a_w != b_w ) {
        return c3n;
      }
      else {
        c3_w i_w;

        //  XX memcmp
        //
        for ( i_w = 0; i_w < a_w; i_w++ ) {
          if ( a_u->buf_w[i_w] != b_u->buf_w[i_w] ) {
            return c3n;
          }
        }
      }
    }
  }

  return c3y;
}

/* the lose lists: values that unification unpointed from slots that
**   counted them, each gained by the slot's new value if that needs a
**   count. a value is lost once the road it's on is current again,
**   after the takes of its child's exit (u3r_wed_drain()): nothing
**   junior holds it then except through what was taken, which gained it.
**
**   home values (from _cr_wed_fall()) and values on other inner roads
**   (from u3a_wed(), merging on a senior road) are kept apart: the
**   home list is drained at the exit to home, and the inner list at
**   every other exit, for the entries of the road exited to. after a
**   signal reset the inner list is dropped with the roads it named, and
**   the home list drained. off the loom, allocated on the home road; an
**   entry is written, then published by bumping the length, so an
**   interrupt at worst leaks.
*/
//  NB: the starting sizes and ceilings are first guesses; revisit them
//  with readings from more ships.
//
#define _CR_LO_MIN  (1U << 14)   //  entries, to start
#define _CR_LO_MAX  (1U << 22)   //  entries, at most

typedef struct {
  u3_post rod_p;                 //  the road the value is on
  u3_noun val;
} _cr_lo_inner;

static struct {
  c3_t    ini_t;
  u3c_arr ent_u;   //  home values to lose
  c3_w    len_w;   //  ... published
  u3c_arr inn_u;   //  inner-road values to lose
  c3_w    inn_w;   //  ... published
} _cr_lo;

/* _cr_lo_drain_home(): lose the home values on the list.
*/
static void
_cr_lo_drain_home(void)
{
  c3_w     len_w = _cr_lo.len_w;
  u3_noun* lis_u;
  c3_w     i_w;

  if ( !len_w ) return;
  lis_u = u3c_arr_at(&_cr_lo.ent_u, 0);
  if ( len_w > u3e_Meter.wlp_d ) u3e_Meter.wlp_d = len_w;
  u3e_Meter.wll_d += len_w;

  //  each value is taken off the list before it's lost, so that an
  //  interrupt can't lose it twice
  //
  for ( i_w = len_w; i_w > 0; i_w-- ) {
    u3_noun old = lis_u[i_w - 1];
    _cr_lo.len_w = i_w - 1;
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    u3z(old);
  }
  u3c_arr_shrink(&_cr_lo.ent_u, len_w);
}

/* _cr_lo_drain_inner(): lose the values on this inner road.
*/
static void
_cr_lo_drain_inner(void)
{
  c3_w          len_w = _cr_lo.inn_w;
  u3_post       rod_p = u3of(u3a_road, u3R);
  _cr_lo_inner* lis_u;
  c3_w          kep_w = 0, i_w;

  if ( !len_w ) return;
  lis_u = u3c_arr_at(&_cr_lo.inn_u, 0);
  u3c_arr_used(&_cr_lo.inn_u, len_w);

  //  keep the entries of older roads at the front, and publish them;
  //  then lose this road's, past the end. an interrupt meanwhile drops
  //  the whole list at the signal reset, with the roads it names
  //
  for ( i_w = 0; i_w < len_w; i_w++ ) {
    if ( lis_u[i_w].rod_p != rod_p ) {
      _cr_lo_inner tmp_u = lis_u[kep_w];
      lis_u[kep_w++] = lis_u[i_w];
      lis_u[i_w]     = tmp_u;
    }
  }
  __atomic_signal_fence(__ATOMIC_SEQ_CST);
  _cr_lo.inn_w = kep_w;
  __atomic_signal_fence(__ATOMIC_SEQ_CST);

  u3e_Meter.wvl_d += len_w - kep_w;
  for ( i_w = kep_w; i_w < len_w; i_w++ ) {
    u3z(lis_u[i_w].val);
  }
}

/* u3r_wed_drain(): lose the values unification unpointed from slots on
**                  this road. after every take of an exit to it.
*/
void
u3r_wed_drain(void)
{
  if ( &(u3H->rod_u) == u3R ) {
    _cr_lo_drain_home();
  }
  else {
    _cr_lo_drain_inner();
  }
}

/* _cr_lo_leak_home(): a home value that couldn't go on the full list
**   is leaked: the next grab, which counts references, frees it.
*/
static inline void
_cr_lo_leak_home(void)
{
  u3C.wag_h |= u3o_check_corrupt;
}

/* u3r_wed_room(): there's room on [rod_u]'s lose list. under memory
**                 debugging, a unification is refused without it.
*/
c3_t
u3r_wed_room(u3a_road* rod_u)
{
  return ( &(u3H->rod_u) == rod_u )
         ? (0 != u3c_arr_at(&_cr_lo.ent_u, _cr_lo.len_w))
         : (0 != u3c_arr_at(&_cr_lo.inn_u, _cr_lo.inn_w));
}

/* u3r_wed_lose(): lose [old], a value on road [rod_u], once [rod_u] is
**                 current again after its child's exit. past the
**                 ceiling it's leaked: an inner road's heap goes at its
**                 exit, and a home leak asks for a grab.
*/
void
u3r_wed_lose(u3a_road* rod_u, u3_noun old)
{
  if ( &(u3H->rod_u) == rod_u ) {
    u3_noun* lis_u = u3c_arr_at(&_cr_lo.ent_u, _cr_lo.len_w);

    if ( !lis_u ) {
      u3e_Meter.wsk_d[2]++;
      _cr_lo_leak_home();
      return;
    }
    *lis_u = old;
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    _cr_lo.len_w++;
  }
  else {
    _cr_lo_inner* ent_u = u3c_arr_at(&_cr_lo.inn_u, _cr_lo.inn_w);

    if ( !ent_u ) {
      u3e_Meter.wvf_d++;
      return;
    }
    ent_u->rod_p = u3of(u3a_road, rod_u);
    ent_u->val   = old;
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    _cr_lo.inn_w++;
    u3e_Meter.wvc_d++;
  }
}

/* u3r_wed_left(): what an interrupted exit left on the lose lists: drop
**                 the inner list, with its roads, and lose the home
**                 values. home road only, no excursion under way.
*/
void
u3r_wed_left(void)
{
  if ( _cr_lo.inn_w ) {
    u3e_Meter.wvx_d += _cr_lo.inn_w;
    _cr_lo.inn_w = 0;
  }
  if ( _cr_lo.len_w ) {
    u3e_Meter.wlx_d += _cr_lo.len_w;
    _cr_lo_drain_home();
  }
}

/* u3r_wed_size(): restart the lose lists at [min_w] entries, at most
**                 [max_w] (tests). home road only, the lists empty.
*/
void
u3r_wed_size(c3_w min_w, c3_w max_w)
{
  u3c_arr_init(&_cr_lo.ent_u, "lose list", sizeof(u3_noun), min_w, max_w);
  u3c_arr_init(&_cr_lo.inn_u, "inner lose list",
               sizeof(_cr_lo_inner), min_w, max_w);
  _cr_lo.len_w = _cr_lo.inn_w = 0;
  _cr_lo.ini_t = 1;
}

/* _cr_lo_leap(): a new excursion: allocate, and lose what an interrupted
**   exit left. home road only.
*/
static void
_cr_lo_leap(void)
{
  if ( !_cr_lo.ini_t ) {
    u3c_arr_init(&_cr_lo.ent_u, "lose list",
                 sizeof(u3_noun), _CR_LO_MIN, _CR_LO_MAX);
    u3c_arr_init(&_cr_lo.inn_u, "inner lose list",
                 sizeof(_cr_lo_inner), _CR_LO_MIN, _CR_LO_MAX);
    _cr_lo.ini_t = 1;
  }
  u3r_wed_left();
}

/* _cr_home_slot(): [sot] is in the home road's heap. during an excursion
**   the inner roads are carved from the home road's free space, between
**   its hat and cap, so the heap is bounded by the hat, not the cap.
*/
static inline c3_t
_cr_home_slot(u3_noun* sot)
{
  u3a_road* hom_u = &(u3H->rod_u);
  c3_d      off_d = (c3_d)u3a_outa(sot);

  return ( c3y == u3a_is_north(hom_u) )
         ? ((off_d >= hom_u->rut_p) && (off_d < hom_u->hat_p))
         : ((off_d >= hom_u->hat_p) && (off_d < hom_u->rut_p));
}

/* _cr_slot_on(): [sot] is in [rod_u]'s heap. the offset stays wide, so
**   a slot off the loom is never on a road.
*/
static inline c3_t
_cr_slot_on(u3a_road* rod_u, u3_noun* sot)
{
  c3_d off_d = (c3_d)u3a_outa(sot);

  return ( c3y == u3a_is_north(rod_u) )
         ? ((off_d >= rod_u->rut_p) && (off_d < rod_u->hat_p))
         : ((off_d >= rod_u->hat_p) && (off_d < rod_u->rut_p));
}

/* _cr_wed_where(): where [sot] can be written from an inner road, for a
**   home value: 1 this road's heap, or its child's while it's promoted
**   (on this road's stack), 2 home memory, 3 the heap of an inner road
**   senior to this one; 0 nowhere (off the loom, an older road's stack).
*/
static inline c3_y
_cr_wed_where(u3_noun* sot)
{
  u3a_road* rod_u = u3R;

  if ( _cr_slot_on(rod_u, sot) ) return 1;
  if ( rod_u->kid_p ) {
    c3_d off_d = (c3_d)u3a_outa(sot);

    if ( ( c3y == u3a_is_north(rod_u) )
         ? ((off_d >= rod_u->cap_p) && (off_d < rod_u->mat_p))
         : ((off_d >= rod_u->mat_p) && (off_d < rod_u->cap_p)) )
    {
      return 1;
    }
  }
  if ( _cr_home_slot(sot) )      return 2;

  while ( rod_u->par_p ) {
    rod_u = u3to(u3a_road, rod_u->par_p);
    if ( &(u3H->rod_u) == rod_u ) break;
    if ( _cr_slot_on(rod_u, sot) ) return 3;
  }
  return 0;
}

/* _cr_wed_keep(): of equal home nouns [a] and [b], keep [a]: as on the
**   home road, a member, else the deeper address. a total order, so a
**   slot only ever moves toward the same value.
*/
static inline c3_t
_cr_wed_keep(u3_noun a, u3_noun b)
{
  c3_t amb_t = u3a_hcon_has(a), bmb_t = u3a_hcon_has(b);

  if ( amb_t != bmb_t ) return amb_t;
  return (a < b) == (c3y == u3a_is_north(&(u3H->rod_u)));
}

/* _cr_wed_home(): point home slot [sot] at home noun [val], gaining it
**   on the home road; the old value goes on the lose list. past the
**   list's ceiling the old value is leaked, for the next grab, or under
**   memory debugging the slot is left. 0 if not written: so, or [val]'s
**   count is at its limit.
**
**   the gain, the write and the list entry are made with async signals
**   held (u3m_hold()), so a timeout or an interrupt can't land between
**   them; the sampler skips while they're held too. nothing in between
**   calls a function, so nothing there can bail or overflow the stack.
*/
static c3_t
_cr_wed_home(u3_noun* sot, u3_noun val)
{
  u3_noun*  lis_u = u3c_arr_at(&_cr_lo.ent_u, _cr_lo.len_w);
  u3a_noun* box_u = u3a_to_ptr(val);
  u3_noun   old   = *sot;

  if ( !lis_u ) {
    u3e_Meter.wsk_d[2]++;
    if ( u3a_dbg_ram() ) return 0;
  }

  //  as _me_gain_use(): a home noun's count can't be 0 while it's held;
  //  at the limit, leave the slot as it is. gains from an inner road are
  //  never deferred (see u3a_defer_open())
  //
  if ( 0 == box_u->use_w ) {
    u3m_bail(c3__foul);
  }
  if ( u3a_direct_max == box_u->use_w ) {
    u3e_Meter.wsk_d[2]++;
    return 0;
  }

  u3m_hold();
  box_u->use_w += 1;
  *sot = val;
  if ( lis_u ) {
    *lis_u = old;
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    _cr_lo.len_w++;
  }
  else {
    _cr_lo_leak_home();
  }
  u3m_hold_done();
  return 1;
}

/* _cr_wed_fall(): u3a_wed() couldn't merge equal [*a] and [*b] on any
**   road below home: both values are senior to every road it could merge
**   them on. on an inner road, not promoting, point one slot at the
**   other's value: keep the value _cr_wed_keep() prefers, if both slots
**   can be written, else write the slot that can. a slot in the heap of
**   this road, or of an inner road senior to it, is written as is
**   (references from inner roads into home memory aren't counted); a
**   slot in home memory gains its new value on home, and the old one is
**   lost after the exit's takes. a slot holding a member is never
**   written: the home set is closed, so it may be in one.
*/
static void
_cr_wed_fall(u3_noun* a, u3_noun* b)
{
  c3_y awr_y, bwr_y;
  c3_t kep_t;

  if ( &(u3H->rod_u) == u3R ) return;

  u3e_Meter.wsg_d++;
  if ( u3R->kid_p ) u3e_Meter.wsk_d[0]++;
  awr_y = _cr_wed_where(a);
  bwr_y = _cr_wed_where(b);

  if ( !awr_y && !bwr_y ) {
    u3e_Meter.wsn_d++;
    return;
  }
  else if ( awr_y && bwr_y ) {
    kep_t = _cr_wed_keep(*a, *b);
  }
  else {
    kep_t = ( 0 != bwr_y );

    //  the one slot we can write holds the value the order prefers
    //
    if ( kep_t != _cr_wed_keep(*a, *b) ) {
      u3e_Meter.wso_d++;
    }
  }

  {
    u3_noun* sot   = kep_t ? b : a;
    u3_noun  val   = kep_t ? *a : *b;
    c3_y     wri_y = kep_t ? bwr_y : awr_y;

    if ( u3a_hcon_has(*sot) ) {
      u3e_Meter.wsk_d[1]++;
      return;
    }
    if ( 2 != wri_y ) {
      *sot = val;
      u3e_Meter.wsw_d[( 1 == wri_y ) ? 0 : 2]++;
    }
    else if ( _cr_wed_home(sot, val) ) {
      u3e_Meter.wsw_d[1]++;
    }
  }
}

/* the pair table: pairs of cells found equal at different addresses,
**   which unification couldn't merge. it bounds long comparisons where
**   unification can't; on the home road, unification does.
**
**   the excursion table holds pairs of home cells, found on inner roads.
**   home nouns are neither freed nor changed during an excursion from
**   the home road, so an entry stays true until the excursion ends, and
**   it's shared by every comparison until then; the next excursion
**   clears it. while a junior road is promoted ([kid_p] set), or on an
**   inner road while debugging memory, unification doesn't reach the
**   pairs a comparison meets, and nouns may be freed between
**   comparisons: a long comparison records any pairs it can in the
**   scratch table, cleared at its start and end.
**
**   both grow as needed, up to a ceiling (see crib.h), so a comparison
**   stays linear unless it needs more than that.
*/
#define _CR_PM_TOP   64      //  on an inner road, probe the root after this many steps
#define _CR_PM_DEEP  65535   //  then probe inner pairs after this many
#define _CR_PM_LOG   14      //  entries, log2, to start
#define _CR_PM_MAX   23      //  ... at most (NB: a first guess, to revisit)

static c3_t    _cr_pm_ini_t;
static u3c_set _cr_pm_x;   //  excursion: home pairs
static u3c_set _cr_pm_s;   //  scratch: one comparison

/* _cr_sing_ctx: where a long comparison runs, found when it gets long.
*/
typedef struct {
  c3_y      scp_y;   //  0: home road, 1: inner road, 2: promoting a junior road
  c3_y      pmo_y;   //  pairs recorded: 1 home, 2 senior, 3 any
  u3c_set*  pms_u;   //  ... in this table
  u3a_road* one_u;   //  the outermost inner road
  c3_t      prt_t;   //  record the root pair if equal
  u3_noun   ra, rb;  //  the root pair
} _cr_sing_ctx;

/* u3r_pair_size(): restart the pair tables at 2^[log_y] entries
**                  (at least 16), growing as before (tests). home road
**                  only.
*/
void
u3r_pair_size(c3_y log_y)
{
  u3c_set_init(&_cr_pm_x, "pair table", log_y, _CR_PM_MAX);
  u3c_set_init(&_cr_pm_s, "pair scratch table", log_y, _CR_PM_MAX);
  _cr_pm_ini_t = 1;
}

/* u3r_pair_cap(): the excursion table's capacity (tests).
*/
c3_d
u3r_pair_cap(void)
{
  return u3c_set_cap(&_cr_pm_x);
}

/* u3r_sing_leap(): a new excursion from the home road. home road only.
*/
void
u3r_sing_leap(void)
{
  if ( !_cr_pm_ini_t ) {
    u3r_pair_size(_CR_PM_LOG);
  }
  u3c_set_shrink(&_cr_pm_x);
  u3c_set_shrink(&_cr_pm_s);
  _cr_lo_leap();
}

/* u3r_crib_report(): log the pair tables' and deferral's evictions and
**                    drops at their ceilings. home road only, between
**                    events.
*/
void
u3r_crib_report(void)
{
  if ( _cr_pm_ini_t ) {
    u3c_set_report(&_cr_pm_x);
    u3c_set_report(&_cr_pm_s);
  }
  if ( _cr_lo.ini_t ) {
    u3c_arr_report(&_cr_lo.ent_u);
    u3c_arr_report(&_cr_lo.inn_u);
  }
}

/* u3r_crib_stat(): the pair tables' and deferral's use since the last
**                  call, into [sat_u] (room for 4); how many.
*/
c3_w
u3r_crib_stat(u3c_stat* sat_u)
{
  c3_w len_w = 0;

  if ( _cr_pm_ini_t ) {
    sat_u[len_w++] = u3c_set_stat(&_cr_pm_x);
    sat_u[len_w++] = u3c_set_stat(&_cr_pm_s);
  }
  if ( _cr_lo.ini_t ) {
    sat_u[len_w++] = u3c_arr_stat(&_cr_lo.ent_u);
    sat_u[len_w++] = u3c_arr_stat(&_cr_lo.inn_u);
  }
  return len_w;
}

/* _cr_pm_has(): [a] and [b] are known to be equal.
*/
static inline c3_t
_cr_pm_has(u3c_set* set_u, u3_noun a, u3_noun b)
{
  if ( a > b ) { u3_noun t = a; a = b; b = t; }
  return u3c_set_has(set_u, a, b);
}

/* _cr_pm_find(): _cr_pm_has() for an inner pair, counted.
*/
static inline c3_t
_cr_pm_find(u3c_set* set_u, u3_noun a, u3_noun b)
{
  u3e_Meter.pmp_d[0]++;
  if ( _cr_pm_has(set_u, a, b) ) {
    u3e_Meter.pmp_d[1]++;
    return 1;
  }
  return 0;
}

/* _cr_pm_put(): record that [a] and [b] are equal.
*/
static inline void
_cr_pm_put(u3c_set* set_u, u3_noun a, u3_noun b)
{
  if ( a > b ) { u3_noun t = a; a = b; b = t; }
  u3c_set_put(set_u, a, b);
}

/* _cr_sing_scope(): where this comparison runs.
*/
static void
_cr_sing_scope(_cr_sing_ctx* ctx_u)
{
  if ( u3R->kid_p ) {
    ctx_u->scp_y = 2;
  }
  else if ( &(u3H->rod_u) != u3R ) {
    ctx_u->scp_y = 1;
    ctx_u->one_u = u3to(u3a_road, u3H->rod_u.kid_p);
  }
  else {
    ctx_u->scp_y = 0;
  }
}

/* _cr_home(): [som] is a home noun, on an inner road.
*/
static inline c3_t
_cr_home(_cr_sing_ctx* ctx_u, u3_noun som)
{
  return c3y == u3a_is_senior(ctx_u->one_u, som);
}

/* _cr_pm_ok(): equal cells [a] and [b] can go in the pair table, by
**   [pmo_y]. on an inner road, pairs of home cells, which nothing frees
**   during the excursion. debugging memory, pairs senior to this road,
**   which unification here doesn't free. while a junior road is
**   promoted, nothing is unified or freed during a comparison.
*/
static inline __attribute__((always_inline)) c3_t
_cr_pm_ok(_cr_sing_ctx* ctx_u, u3_noun a, u3_noun b)
{
  switch ( ctx_u->pmo_y ) {
    case 1:  return _cr_home(ctx_u, a) && _cr_home(ctx_u, b);
    case 2:  return (c3y == u3a_is_senior(u3R, a))
                 && (c3y == u3a_is_senior(u3R, b));
    default: return 1;
  }
}

/* _cr_sing_wed(): unify the children [*a] and [*b] of equal cells,
**   through a slot or at the exit home if they can't be merged.
*/
static inline __attribute__((always_inline)) void
_cr_sing_wed(u3_noun *restrict a, u3_noun *restrict b)
{
  if ( (*a != *b) && (c3n == u3a_wed(a, b)) ) {
    _cr_wed_fall(a, b);
  }
}

/* _cr_sing_deep(): unifying equality, continued with the pair table
 *                  (tightly coupled to _cr_sing). not on the home road,
 *                  which unifies instead.
 */
static c3_o
_cr_sing_deep(u3a_pile* pil_u, _cr_sing_ctx* ctx_u)
{
  eqframe*  fam_u = u3a_peek(pil_u);
  c3_o      ret_o = c3y;
  u3_noun   a, b;
  u3a_cell* a_u;
  u3a_cell* b_u;

  u3_assert( _cr_pm_ini_t );

  //  which pairs to record. where unification merges none, record any,
  //  for this comparison only
  //
  ctx_u->pmo_y = ( 2 == ctx_u->scp_y ) ? 3 : 1;
  ctx_u->pms_u = ( 1 == ctx_u->pmo_y ) ? &_cr_pm_x : &_cr_pm_s;
  u3e_Meter.pmd_d[ctx_u->pmo_y - 1]++;
  if ( 1 < ctx_u->pmo_y ) {
    u3c_set_clear(&_cr_pm_s);
  }

  //  loop while arguments remain on the stack
  //
  do {
    a = fam_u->a;
    b = fam_u->b;

    switch ( fam_u->sat_y ) {

      //  [a] and [b] are arbitrary nouns
      //
      case SING_NONE: {
        if ( a == b ) {
          break;
        }
        else if ( c3y == u3a_is_atom(a) ) {
          if ( c3n == _cr_sing_atom(a, b) ) {
            ret_o = c3n;
            goto done;
          }
          else {
            break;
          }
        }
        else if ( c3y == u3a_is_atom(b) ) {
          ret_o = c3n;
          goto done;
        }
        //  [a] and [b] are cells
        //
        else {
          a_u = u3a_to_ptr(a);
          b_u = u3a_to_ptr(b);

          //  short-circuiting mug check
          //
          if ( c3n == _cr_sing_mug((u3a_noun*)a_u, (u3a_noun*)b_u) ) {
            ret_o = c3n;
            goto done;
          }
          //  a pair known to be equal
          //
          else if ( _cr_pm_ok(ctx_u, a, b) && _cr_pm_find(ctx_u->pms_u, a, b) ) {
            fam_u = u3a_pop(pil_u);
            continue;
          }
          //  upgrade none-frame to head-frame, check heads
          //
          else {
            fam_u->sat_y = SING_HEAD;
            fam_u = _cr_sing_push(pil_u, a_u->hed, b_u->hed);
            continue;
          }
        }
      } break;

      //  cells [a] and [b] have equal heads
      //
      case SING_HEAD: {
        a_u = u3a_to_ptr(a);
        b_u = u3a_to_ptr(b);
        _cr_sing_wed(&(a_u->hed), &(b_u->hed));

        //  upgrade head-frame to tail-frame, check tails
        //
        fam_u->sat_y = SING_TAIL;
        fam_u = _cr_sing_push(pil_u, a_u->tel, b_u->tel);
        continue;
      }

      //  cells [a] and [b] are equal
      //
      case SING_TAIL: {
        a_u = u3a_to_ptr(a);
        b_u = u3a_to_ptr(b);
        _cr_sing_wed(&(a_u->tel), &(b_u->tel));
      } break;

      default: {
        u3_assert(0);
      } break;
    }

    //  record equal cells, to short-circuit their re-comparison
    //
    if ( (SING_TAIL == fam_u->sat_y) && (a != b) && _cr_pm_ok(ctx_u, a, b) ) {
      _cr_pm_put(ctx_u->pms_u, a, b);
      u3e_Meter.pmp_d[2]++;
    }

    fam_u = u3a_pop(pil_u);
  }
  while ( c3n == u3a_pile_done(pil_u) );

done:
  if ( 1 < ctx_u->pmo_y ) {
    u3c_set_clear(&_cr_pm_s);
  }
  return ret_o;
}

/* _cr_sing(): unifying equality.
**
**   on the home road, unification merges every equal pair it compares,
**   so comparing a pair again is cheap and comparisons stay linear.
**   elsewhere, [ovr_s] counts steps: on an inner road, after _CR_PM_TOP
**   steps the root pair is looked up in the pair table, if it's two
**   home cells; after _CR_PM_DEEP, the comparison continues in
**   _cr_sing_deep(), which looks up and records the pairs unification
**   can't merge.
*/
static c3_o
_cr_sing(u3_noun a, u3_noun b)
{
  c3_s         ovr_s = (c3_s)(0x10000 - _CR_PM_TOP);
  c3_t         stg_t = 0;   //  the root has been looked up
  _cr_sing_ctx ctx_u;
  u3a_cell*    a_u;
  u3a_cell*    b_u;
  eqframe*     fam_u;
  u3a_pile     pil_u;

  ctx_u.prt_t = 0;

  //  initialize stack control, push arguments onto the stack (none-frame)
  //
  u3a_pile_prep(&pil_u, sizeof(eqframe), alignof(eqframe));
  fam_u = _cr_sing_push(&pil_u, a, b);

  //  loop while arguments are on the stack
  //
  while ( c3n == u3a_pile_done(&pil_u) ) {
    a = fam_u->a;
    b = fam_u->b;

    switch ( fam_u->sat_y ) {

      //  [a] and [b] are arbitrary nouns
      //
      case SING_NONE: {
        if ( a == b ) {
          break;
        }
        else if ( c3y == u3a_is_atom(a) ) {
          if ( c3n == _cr_sing_atom(a, b) ) {
            u3R->cap_p = pil_u.top_p;
            return c3n;
          }
          else {
            break;
          }
        }
        else if ( c3y == u3a_is_atom(b) ) {
          u3R->cap_p = pil_u.top_p;
          return c3n;
        }
        //  [a] and [b] are cells
        //
        else {
          a_u = u3a_to_ptr(a);
          b_u = u3a_to_ptr(b);

          //  short-circuiting mug check
          //
          if ( c3n == _cr_sing_mug((u3a_noun*)a_u, (u3a_noun*)b_u) ) {
            u3R->cap_p = pil_u.top_p;
            return c3n;
          }
          //  upgrade none-frame to head-frame, check heads
          //
          else {
            fam_u->sat_y = SING_HEAD;
            fam_u = _cr_sing_push(&pil_u, a_u->hed, b_u->hed);
            continue;
          }
        }
      } break;

      //  cells [a] and [b] have equal heads
      //
      case SING_HEAD: {
        a_u = u3a_to_ptr(a);
        b_u = u3a_to_ptr(b);
        _cr_sing_wed(&(a_u->hed), &(b_u->hed));

        //  upgrade head-frame to tail-frame, check tails
        //
        fam_u->sat_y = SING_TAIL;
        fam_u = _cr_sing_push(&pil_u, a_u->tel, b_u->tel);
        continue;
      }

      //  cells [a] and [b] are equal
      //
      case SING_TAIL: {
        a_u = u3a_to_ptr(a);
        b_u = u3a_to_ptr(b);
        _cr_sing_wed(&(a_u->tel), &(b_u->tel));
      } break;

      default: {
        u3_assert(0);
      } break;
    }

    if ( 0 == ++ovr_s ) {
      if ( !stg_t ) {
        //  a long comparison: on an inner road, look up the root pair,
        //  the stack's first frame
        //
        eqframe* rot_u = u3to(eqframe, pil_u.top_p + pil_u.mov_ws
                                                   + pil_u.off_ws);
        u3_noun  ra    = rot_u->a, rb = rot_u->b;

        stg_t = 1;
        ovr_s = (c3_s)(0x10000 - (_CR_PM_DEEP - _CR_PM_TOP));
        _cr_sing_scope(&ctx_u);
        u3e_Meter.pml_d[ctx_u.scp_y]++;

        if (  (1 == ctx_u.scp_y) && (ra != rb)
           && (c3y == u3a_is_cell(ra)) && (c3y == u3a_is_cell(rb))
           && _cr_home(&ctx_u, ra) && _cr_home(&ctx_u, rb) )
        {
          u3_assert( _cr_pm_ini_t );

          u3e_Meter.pmr_d[0]++;
          if ( _cr_pm_has(&_cr_pm_x, ra, rb) ) {
            u3e_Meter.pmr_d[1]++;
            u3R->cap_p = pil_u.top_p;
            return c3y;
          }
          ctx_u.prt_t = 1;
          ctx_u.ra    = ra;
          ctx_u.rb    = rb;
        }
      }
      //  a very long comparison, not on the home road
      //
      else if ( ctx_u.scp_y ) {
        c3_o ret_o = _cr_sing_deep(&pil_u, &ctx_u);

        u3R->cap_p = pil_u.top_p;
        if ( ctx_u.prt_t && (c3y == ret_o) ) {
          _cr_pm_put(&_cr_pm_x, ctx_u.ra, ctx_u.rb);
          u3e_Meter.pmr_d[2]++;
        }
        return ret_o;
      }
    }

    fam_u = u3a_pop(&pil_u);
  }

  if ( ctx_u.prt_t ) {
    _cr_pm_put(&_cr_pm_x, ctx_u.ra, ctx_u.rb);
    u3e_Meter.pmr_d[2]++;
  }
  return c3y;
}

/* u3r_sing(): Yes iff [a] and [b] are the same noun.
*/
c3_o
u3r_sing_imp(u3_noun a, u3_noun b)
{
  c3_o ret_o;
  u3t_on(euq_o);
  ret_o = _cr_sing(a, b);
  u3t_off(euq_o);
  return ret_o;
}

/* u3r_sing_wed(): u3r_sing() on two slots that own their references,
**   unifying the slots themselves when equal, as unification does for
**   the children it compares.
*/
c3_o
u3r_sing_wed_imp(u3_noun* a, u3_noun* b)
{
  if ( *a == *b ) {
    return c3y;
  }

  u3e_Meter.jck_d[0]++;
  if ( c3n == u3r_sing(*a, *b) ) {
    u3e_Meter.jck_d[1]++;
    return c3n;
  }
  else if ( *a != *b ) {
    //  cores sharing a call site converge on one battery: each check
    //  keeps the value _cr_wed_keep() prefers, in the cache or the core
    //
    if ( c3n == u3a_wed(a, b) ) {
      _cr_wed_fall(a, b);
    }
    u3e_Meter.jck_d[( *a == *b ) ? 2 : 3]++;
  }
  return c3y;
}

c3_o
u3r_fing(u3_noun a,
           u3_noun b)
{
  return (a == b) ? c3y : c3n;
}

/* u3r_sing_cell():
**
**   Yes iff `[p q]` and `b` are the same noun.
*/
c3_o
u3r_sing_cell(u3_noun p,
                u3_noun q,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_sing(p, u3a_h(b)),
                       u3r_sing(q, u3a_t(b))));
}
c3_o
u3r_fing_cell(u3_noun p,
                u3_noun q,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_fing(p, u3a_h(b)),
                       u3r_fing(q, u3a_t(b))));
}

/* u3r_sing_mixt():
**
**   Yes iff `[p q]` and `b` are the same noun.
*/
c3_o
u3r_sing_mixt(const c3_c* p_c,
                u3_noun     q,
                u3_noun     b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_sing_c(p_c, u3a_h(b)),
                       u3r_sing(q, u3a_t(b))));
}
c3_o
u3r_fing_mixt(const c3_c* p_c,
                u3_noun     q,
                u3_noun     b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_sing_c(p_c, u3a_h(b)),
                       u3r_fing(q, u3a_t(b))));
}

/* u3r_sing_trel():
**
**   Yes iff `[p q r]` and `b` are the same noun.
*/
c3_o
u3r_sing_trel(u3_noun p,
                u3_noun q,
                u3_noun r,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_sing(p, u3a_h(b)),
                       u3r_sing_cell(q, r, u3a_t(b))));
}
c3_o
u3r_fing_trel(u3_noun p,
                u3_noun q,
                u3_noun r,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_fing(p, u3a_h(b)),
                       u3r_fing_cell(q, r, u3a_t(b))));
}

/* u3r_sing_qual():
**
**   Yes iff `[p q r]` and `b` are the same noun.
*/
c3_o
u3r_sing_qual(u3_noun p,
                u3_noun q,
                u3_noun r,
                u3_noun s,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_sing(p, u3a_h(b)),
                       u3r_sing_trel(q, r, s, u3a_t(b))));
}
c3_o
u3r_fing_qual(u3_noun p,
                u3_noun q,
                u3_noun r,
                u3_noun s,
                u3_noun b)
{
  return c3a(_(u3a_is_cell(b)),
                c3a(u3r_fing(p, u3a_h(b)),
                       u3r_fing_trel(q, r, s, u3a_t(b))));
}

/* u3r_nord():
**
**   Return 0, 1 or 2 if `a` is below, equal to, or above `b`.
*/
u3_atom
u3r_nord(u3_noun a,
        u3_noun b)
{
  u3_assert(u3_none != a);
  u3_assert(u3_none != b);

  if ( a == b ) {
    return 1;
  }
  else {
    if ( _(u3a_is_atom(a)) ) {
      if ( !_(u3a_is_atom(b)) ) {
        return 0;
      } else {
        if ( _(u3a_is_cat(a)) ) {
          if ( _(u3a_is_cat(b)) ) {
            return (a < b) ? 0 : 2;
          }
          else return 0;
        }
        else if ( _(u3a_is_cat(b)) ) {
          return 2;
        }
        else {
          u3a_atom* a_u = u3a_to_ptr(a);
          u3a_atom* b_u = u3a_to_ptr(b);

          c3_w w_rez = a_u->len_w;
          c3_w w_mox = b_u->len_w;

          if ( w_rez != w_mox ) {
            return (w_rez < w_mox) ? 0 : 2;
          }
          else {
            c3_w i_w;

            for ( i_w = 0; i_w < w_rez; i_w++ ) {
              c3_w ai_w = a_u->buf_w[i_w];
              c3_w bi_w = b_u->buf_w[i_w];

              if ( ai_w != bi_w ) {
                return (ai_w < bi_w) ? 0 : 2;
              }
            }
            return 1;
          }
        }
      }
    } else {
      if ( _(u3a_is_atom(b)) ) {
        return 2;
      } else {
        u3_atom c = u3r_nord(u3a_h(a), u3a_h(b));

        if ( 1 == c ) {
          return u3r_nord(u3a_t(a), u3a_t(b));
        } else {
          return c;
        }
      }
    }
  }
}

/* u3r_sing_c(): cord/C-string value equivalence.
*/
c3_o
u3r_sing_c(const c3_c* a_c,
           u3_noun     b)
{
  u3_assert(u3_none != b);

  if ( !_(u3a_is_atom(b)) ) {
    return c3n;
  }
  else {
    c3_w w_sof = strlen(a_c);
    c3_w i_w;

    if ( w_sof != u3r_met(3, b) ) {
      return c3n;
    }
    for ( i_w = 0; i_w < w_sof; i_w++ ) {
      if ( u3r_byte(i_w, b) != a_c[i_w] ) {
        return c3n;
      }
    }
    return c3y;
  }
}

/* u3r_bush():
**
**   Factor [a] as a bush [b.[p q] c].
*/
c3_o
u3r_bush(u3_noun  a,
           u3_noun* b,
           u3_noun* c)
{
  u3_assert(u3_none != a);

  if ( _(u3a_is_atom(a)) ) {
    return c3n;
  }
  else {
    *b = u3a_h(a);

    if ( _(u3a_is_atom(*b)) ) {
      return c3n;
    } else {
      *c = u3a_t(a);
      return c3y;
    }
  }
}

/* u3r_bite(): retrieve/default $bloq and $step from $bite.
*/
c3_o
u3r_bite(u3_noun bite, u3_atom* bloq, u3_atom *step)
{
  u3_noun hed, tal;

  if ( c3n == u3r_cell(bite, &hed, &tal) ) {
    *bloq = bite;
    *step = 1;
    return c3y;
  }
  else if (  (c3n == u3a_is_atom(hed))
          || (c3n == u3a_is_atom(tal)) )
  {
    return c3n;
  }
  else {
    *bloq = hed;
    *step = tal;
    return c3y;
  }
}

/* u3r_p():
**
**   & [0] if [a] is of the form [b *c].
*/
c3_o
u3r_p(u3_noun  a,
        u3_noun  b,
        u3_noun* c)
{
  u3_noun feg, nux;

  if ( (c3y == u3r_cell(a, &feg, &nux)) &&
       (c3y == u3r_sing(feg, b)) )
  {
    if ( c ) *c = nux;
    return c3y;
  }
  else return c3n;
}

/* u3r_pq():
**
**   & [0] if [a] is of the form [b *c d].
*/
c3_o
u3r_pq(u3_noun  a,
         u3_noun  b,
         u3_noun* c,
         u3_noun* d)
{
  u3_noun nux;

  if ( (c3y == u3r_p(a, b, &nux)) &&
       (c3y == u3r_cell(nux, c, d)) )
  {
    return c3y;
  }
  else return c3n;
}

/* u3r_pqr():
**
**   & [0] if [a] is of the form [b *c *d *e].
*/
c3_o
u3r_pqr(u3_noun  a,
          u3_noun  b,
          u3_noun* c,
          u3_noun* d,
          u3_noun* e)
{
  u3_noun nux;

  if ( (c3y == u3r_p(a, b, &nux)) &&
       (c3y == u3r_trel(nux, c, d, e)) )
  {
    return c3y;
  }
  else return c3n;
}

/* u3r_pqrs():
**
**   & [0] if [a] is of the form [b *c *d *e *f].
*/
c3_o
u3r_pqrs(u3_noun  a,
           u3_noun  b,
           u3_noun* c,
           u3_noun* d,
           u3_noun* e,
           u3_noun* f)
{
  u3_noun nux;

  if ( (c3y == u3r_p(a, b, &nux)) &&
       (c3y == u3r_qual(nux, c, d, e, f)) )
  {
    return c3y;
  }
  else return c3n;
}

/* u3r_met():
**
**   Return the size of (b) in bits, rounded up to
**   (1 << a_y).
**
**   For example, (a_y == 3) returns the size in bytes.
**   NB: (a_y) must be < 37.
*/
// XX: 64 make 64 in 32 bit case too, change all callsites to c3_d
c3_w
u3r_met(c3_y  a_y,
        u3_atom b)
{
  c3_dessert(u3_none != b);
  c3_dessert(_(u3a_is_atom(b)));

  if ( b == 0 ) {
    return 0;
  }
  /* gal_w: number of words besides (daz_w) in (b).
  ** daz_w: top word in (b).
  */
  c3_w gal_w;
  c3_w daz_w;

  if ( _(u3a_is_cat(b)) ) {
    gal_w = 0;
    daz_w = b;
  }
  else {
    u3a_atom* b_u = u3a_to_ptr(b);

    gal_w = (b_u->len_w) - 1;
    daz_w = b_u->buf_w[gal_w];
  }

  /* 5 because 1<<2 bytes in c3_w, 1<<3 bits in byte.
     aka log2(CHAR_BIT * sizeof gal_w)
     a_y < 5 informs whether we shift return left or right
     */
  if (a_y < u3a_word_bits_log) {
    c3_y max_y = (1 << a_y) - 1;
    c3_y gow_y = u3a_word_bits_log - a_y;

    if (gal_w > ((c3_w_max - (u3a_word_bits + max_y)) >> gow_y))
      return u3m_bail(c3__fail);

    return (gal_w << gow_y)
      + ((c3_bits_word(daz_w) + max_y)
         >> a_y);
  }
  c3_y gow_y = (a_y - u3a_word_bits_log);
  return ((gal_w + 1) + ((1 << gow_y) - 1)) >> gow_y;
}

/* u3r_bit():
**
**   Return bit (a_w) of (b).
*/
c3_b
u3r_bit(c3_w    a_w,
          u3_atom b)
{
  u3_assert(u3_none != b);
  u3_assert(_(u3a_is_atom(b)));

  if ( _(u3a_is_cat(b)) ) {
    if ( a_w >= (u3a_word_bits - 1) ) {
      return 0;
    }
    else return (1 & (b >> a_w));
  }
  else {
    u3a_atom* b_u   = u3a_to_ptr(b);
    c3_y        vut_y = (a_w & (u3a_word_bits - 1));
    c3_w        pix_w = (a_w >> u3a_word_bits_log);

    if ( pix_w >= b_u->len_w ) {
      return 0;
    }
    else {
      c3_w nys_w = b_u->buf_w[pix_w];

      return (1 & (nys_w >> vut_y));
    }
  }
}

/* u3r_byte():
**
**   Return byte (a_w) of (b).
*/
c3_y
u3r_byte(c3_w    a_w,
           u3_atom b)
{
  u3_assert(u3_none != b);
  u3_assert(_(u3a_is_atom(b)));

  if ( _(u3a_is_cat(b)) ) {
    if ( a_w > (u3a_word_bytes - 1) ) {
      return 0;
    }
    else return (255 & (b >> (a_w << 3)));
  }
  else {
    u3a_atom* b_u   = u3a_to_ptr(b);
    c3_y      vut_y = (a_w & (u3a_word_bytes - 1));
    c3_w      pix_w = (a_w >> u3a_word_bytes_shift);

    if ( pix_w >= b_u->len_w ) {
      return 0;
    }
    else {
      c3_w nys_w = b_u->buf_w[pix_w];

      return (255 & (nys_w >> (vut_y << 3)));
    }
  }
}

/* u3r_bytes():
**
**  Copy bytes (a_w) through (a_w + b_w - 1) from (d) to (c).
*/
void
u3r_bytes(c3_w    a_w,
          c3_w    b_w,
          c3_y*   c_y,
          u3_atom d)
{
  u3_assert(u3_none != d);
  u3_assert(_(u3a_is_atom(d)));

  c3_y* buf_y;
  c3_w  len_w;

  STATIC_ASSERT(c3_endian == c3_endian_little, "");
  if ( _(u3a_is_cat(d)) ) {
    buf_y = (c3_y*)&d;
    len_w = sizeof(c3_w);
  }
  else {
    u3a_atom* d_u = u3a_to_ptr(d);

    buf_y = (c3_y*)d_u->buf_w;
    len_w = d_u->len_w * sizeof(c3_w);
  }

  {
    c3_w hav_w = ( a_w < len_w ) ? (len_w - a_w) : 0;
    c3_w z_w   = c3_min(b_w, hav_w);

    //  guarded so [buf_y + a_w] is only formed when in bounds
    //
    if ( z_w ) {
      memcpy(c_y, buf_y + a_w, z_w);
    }
    memset(c_y + z_w, 0, b_w - z_w);
  }
}

/* u3r_bytes_fit():
**
**  Copy (len_w) bytes of (a) into (buf_y) if it fits, returning overage
*/
c3_w
u3r_bytes_fit(c3_w len_w, c3_y *buf_y, u3_atom a)
{
  c3_w met_w = u3r_met(3, a);
  if ( met_w <= len_w ) {
    u3r_bytes(0, len_w, buf_y, a);
    return 0;
  }
  else {
    return len_w - met_w;
  }
}

/* u3r_bytes_alloc():
**
**  Copy (len_w) bytes starting at (a_w) from (b) into a fresh allocation.
*/
c3_y*
u3r_bytes_alloc(c3_w    a_w,
                c3_w    len_w,
                u3_atom b)
{
  c3_y* b_y = u3a_malloc(len_w);
  u3r_bytes(a_w, len_w, b_y, b);
  return b_y;
}

/* u3r_bytes_all():
**
**  Allocate and return a new byte array with all the bytes of (a),
**  storing the length in (len_w).
*/
c3_y*
u3r_bytes_all(c3_w* len_w, u3_atom a)
{
  c3_w met_w = *len_w = u3r_met(3, a);
  return u3r_bytes_alloc(0, met_w, a);
}

/* _mpz_init_set_word():
**
**   Initialize (a_mp) from a single word (b_w).
**   Uses mpz_import when unsigned long is narrower than c3_w (e.g. Windows).
*/
static void
_mpz_init_set_word(mpz_t a_mp, c3_w b_w)
{
  if ( sizeof(unsigned long int) >= sizeof(b_w) ) {
    mpz_init_set_ui(a_mp, b_w);
  }
  else {
    mpz_init2(a_mp, u3a_word_bits);
    mpz_import(a_mp, 1, -1, sizeof(c3_w), 0, 0, &b_w);
  }
}

/* u3r_mp():
**
**   Copy (b) into (a_mp).
*/
void
u3r_mp(mpz_t   a_mp,
       u3_atom b)
{
  u3_assert(u3_none != b);
  u3_assert(_(u3a_is_atom(b)));

  if ( _(u3a_is_cat(b)) ) {
    _mpz_init_set_word(a_mp, b);
  }
  else {
    u3a_atom* b_u = u3a_to_ptr(b);
    c3_w    len_w = b_u->len_w;
    c3_d    bit_d = (c3_d)len_w << u3a_word_bits_log;

    //  avoid reallocation on import, if possible
    //
    //mpz_init2(a_mp, (c3_w)c3_min(bit_d, UINT32_MAX));
    assert(bit_d <= c3_w_max);
    mpz_init2(a_mp, bit_d);
    mpz_import(a_mp, len_w, -1, sizeof(c3_w), 0, 0, b_u->buf_w);
  }
}

/* u3r_short():
**
**   Return short (a_w) of (b).
*/
// XX: what
c3_s
u3r_short(c3_w  a_w,
          u3_atom b)
{
  u3_assert( u3_none != b );
  u3_assert( c3y == u3a_is_atom(b) );

  c3_w wor_w;

  if ( c3y == u3a_is_cat(b) ) wor_w = b;
  else {
    u3a_atom* b_u = u3a_to_ptr(b);
    c3_w    nix_w = a_w >> u3a_word_words;

    if ( nix_w >= b_u->len_w ) {
      return 0;
    }
    else {
      wor_w = b_u->buf_w[nix_w];
      a_w &= (1 << u3a_word_words) - 1;
    }
  }

  switch ( a_w ) {
#ifndef VERE64
    case 0:  return wor_w & 0xffff;
    case 1:  return wor_w >> 16;
#else
    case 0:  return wor_w & 0xffff;
    case 1:  return (wor_w >> 16) & 0xffff;
    case 2:  return (wor_w >> 32) & 0xffff;
    case 3:  return wor_w >> 48;
#endif
  }
  return 0; // unreachable, but needed for return in all code paths
}

/* u3r_half():
**
**   Return half (a_w) of (b).
*/
c3_h
u3r_half(c3_w    a_w,
           u3_atom b)
{
  u3_assert(u3_none != b);
  u3_assert(_(u3a_is_atom(b)));

  if ( _(u3a_is_cat(b)) ) {
#ifdef VERE64
    if ( a_w > 1 )
      return 0;
    else
      return (c3_h)((a_w == 0) ? b : b >> u3a_half_bits);
#else
    if ( a_w > 0 )
      return 0;
    else
      return b;
#endif
  }
  else {
    u3a_atom* b_u = u3a_to_ptr(b);
#ifdef VERE64
    if ( a_w >= (b_u->len_w * 2) ) {
#else
    if ( a_w >= b_u->len_w ) {
#endif
      return 0;
    }
    else return ((c3_h*)b_u->buf_w)[a_w];
  }
}

/* u3r_chub():
**
**   Return double-word (a_w) of (b).
*/
c3_d
u3r_chub(c3_w  a_w,
           u3_atom b)
{
#ifndef VERE64
  c3_w wlo_w = u3r_half(a_w * 2, b);
  c3_w whi_w = u3r_half(1 + (a_w * 2), b);

  return (((uint64_t)whi_w) << 32ULL) | ((uint64_t)wlo_w);
#else
  u3_assert(u3_none != b);
  u3_assert(_(u3a_is_atom(b)));

  if ( _(u3a_is_cat(b)) ) {
    if ( a_w > 0 ) {
      return 0;
    }
    else return b;
  }
  else {
    u3a_atom* b_u = u3a_to_ptr(b);

    if ( a_w >= b_u->len_w ) {
      return 0;
    }
    else return ((c3_d*)b_u->buf_w)[a_w];
  }
#endif
}

/* u3r_word():
**
**   Return word (a_w) of (b).
*/
c3_w
u3r_word(c3_w    a_w,
           u3_atom b)
{
#ifndef VERE64
  return u3r_half(a_w, b);
#else
  return u3r_chub(a_w, b);
#endif
}

/* u3r_half_fit():
**
**   Fill (out_w) with (a) if it fits, returning success.
*/
c3_t
u3r_half_fit(c3_h *out_h, u3_atom a)
{
  if ( u3r_met(5, a) > 1 ) {
    return 0;
  }
  else {
    *out_h = u3r_half(0, a);
    return 1;
  }
}

/* u3r_chub_fit():
**
**   Fill (out_w) with (a) if it fits, returning success.
*/
c3_t
u3r_chub_fit(c3_d *out_d, u3_atom a)
{
  if ( u3r_met(6, a) > 1 ) {
    return 0;
  }
  else {
    *out_d = u3r_chub(0, a);
    return 1;
  }
}

/* u3r_chub_fit():
**
**   Fill (out_w) with (a) if it fits, returning success.
*/
c3_t
u3r_word_fit(c3_w *out_w, u3_atom a)
{
#ifndef VERE64
  return u3r_half_fit(out_w, a);
#else
  return u3r_chub_fit(out_w, a);
#endif
}

/* u3r_halfs():
**
**  Copy words (a_w) through (a_w + b_w - 1) from (d) to (c).
*/
void
u3r_halfs(c3_w    a_w,
          c3_w    b_w,
          c3_h*   c_h,
          u3_atom d)
{
  //  XX: assumes little-endian
  //
  u3r_bytes(a_w << 2, b_w << 2, (c3_y*)c_h, d);
}

/* u3r_chubs():
**
**  Copy double-words (a_w) through (a_w + b_w - 1) from (d) to (c).
*/
void
u3r_chubs(c3_w    a_w,
          c3_w    b_w,
          c3_d*   c_d,
          u3_atom d)
{
  //  XX: assumes little-endian
  //
  u3r_bytes(a_w << 3, b_w << 3, (c3_y*)c_d, d);
}

/* u3r_words():
**
**  Copy words (a_w) through (a_w + b_w - 1) from (d) to (c).
*/
void
u3r_words(c3_w    a_w,
          c3_w    b_w,
          c3_w*   c_w,
          u3_atom d)
{
  //  XX: assumes little-endian
  //
  u3r_bytes(a_w << u3a_word_bytes_shift,
            b_w << u3a_word_bytes_shift, (c3_y*)c_w, d);
}

/* u3r_safe_byte(): validate and retrieve byte.
*/
c3_o
u3r_safe_byte(u3_noun dat, c3_y* out_y)
{
  if (  (c3n == u3a_is_atom(dat))
     || (1 < u3r_met(3, dat)) )
  {
    return c3n;
  }

  *out_y = u3r_byte(0, dat);
  return c3y;
}

/* u3r_safe_word(): validate and retrieve word.
*/
c3_o
u3r_safe_half(u3_noun dat, c3_h* out_h)
{
  if (  (c3n == u3a_is_atom(dat))
     || (1 < u3r_met(5, dat)) )
  {
    return c3n;
  }

  *out_h = u3r_half(0, dat);
  return c3y;
}

/* u3r_safe_chub(): validate and retrieve chub.
*/
c3_o
u3r_safe_chub(u3_noun dat, c3_d* out_d)
{
  if (  (c3n == u3a_is_atom(dat))
     || (1 < u3r_met(6, dat)) )
  {
    return c3n;
  }

  *out_d = u3r_chub(0, dat);
  return c3y;
}

/* u3r_safe_chub(): validate and retrieve chub.
*/
c3_o
u3r_safe_word(u3_noun dat, c3_w* out_w)
{
#ifndef VERE64
  return u3r_safe_half(dat, out_w);
#else
  return u3r_safe_chub(dat, out_w);
#endif
}

/* u3r_chop_bits():
**
**   XOR `wid_d` bits from`src_w` at `bif_g` to `dst_w` at `bif_g`
**
**   NB: [dst_w] must have space for [bit_g + wid_d] bits
*/
void
u3r_chop_bits(c3_g  bif_g,
              c3_d  wid_d,
              c3_g  bit_g,
              c3_w* dst_w,
        const c3_w* src_w)
{
  c3_y fib_y = u3a_word_bits - bif_g;
  c3_y tib_y = u3a_word_bits - bit_g;

  //  we need to chop words
  //
  if ( wid_d >= tib_y ) {
    //  align *dst_w
    //
    if ( bit_g ) {
      c3_w low_w = src_w[0] >> bif_g;

      if ( bif_g > bit_g ) {
        low_w   ^= src_w[1] << fib_y;
      }

      *dst_w++  ^= low_w << bit_g;

      wid_d -= tib_y;
      bif_g += tib_y;
      src_w += !!(bif_g >> u3a_word_bits_log);
      bif_g &= (u3a_word_bits - 1);
      fib_y  = u3a_word_bits - bif_g;
    }

    {
      size_t i_i, byt_i = wid_d >> u3a_word_bits_log;

      if ( !bif_g ) {
        for ( i_i = 0; i_i < byt_i; i_i++ ) {
          dst_w[i_i] ^= src_w[i_i];
        }
      }
      else {
        for ( i_i = 0; i_i < byt_i; i_i++ ) {
          dst_w[i_i] ^= (src_w[i_i] >> bif_g) ^ (src_w[i_i + 1] << fib_y);
        }
      }

      src_w += byt_i;
      dst_w += byt_i;
      wid_d &= (u3a_word_bits - 1);
      bit_g  = 0;
    }
  }

  //  we need to chop (more) bits
  //
  if ( wid_d ) {
    c3_w hig_w = src_w[0] >> bif_g;

    if ( wid_d > fib_y ) {
      hig_w   ^= src_w[1] << fib_y;
    }

    *dst_w    ^= (hig_w & (((c3_d)1 << wid_d) - 1)) << bit_g;
  }
}

/* u3r_chop_words():
**
**   Into the bloq space of `met`, from position `fum` for a
**   span of `wid`, to position `tou`, XOR from `src_w`
**   into `dst_w`.
**
**   NB: [dst_w] must have space for [tou_w + wid_w] bloqs
*/
void
u3r_chop_words(c3_g  met_g,
               c3_w  fum_w,
               c3_w  wid_w,
               c3_w  tou_w,
               c3_w* dst_w,
               c3_w  len_w,
         const c3_w* src_w)
{
  //  operate on words
  //
  if ( met_g >= u3a_word_bits_log ) {
    size_t i_i, wid_i;

    {
      c3_g   hut_g = met_g - u3a_word_bits_log;
      size_t fum_i = (size_t)fum_w << hut_g;
      size_t tou_i = (size_t)tou_w << hut_g;
      size_t tot_i;

      wid_i = (size_t)wid_w << hut_g;
      tot_i = fum_i + wid_i;

      //  since [dst_w] must have space for (tou_w + wid_w) bloqs,
      //  neither conversion can overflow
      //
      if ( (fum_i >> hut_g != fum_w) || (tot_i  - wid_i != fum_i) ) {
        u3m_bail(c3__fail);
        return;
      }
      else if ( fum_i >= len_w ) {
        return;
      }

      if ( tot_i > len_w ) {
        wid_i -= tot_i - len_w;
      }

      src_w += fum_i;
      dst_w += tou_i;
    }

    for ( i_i = 0; i_i < wid_i; i_i++ ) {
      dst_w[i_i] ^= src_w[i_i];
    }
  }
  //  operate on bits
  //
  else {
    c3_d wid_d = (c3_d)wid_w << met_g;
    c3_g bif_g, bit_g;

    {
      c3_d len_d = (c3_d)len_w << u3a_word_bits_log;
      c3_d fum_d = (c3_d)fum_w << met_g;
      c3_d tou_d = (c3_d)tou_w << met_g;
      c3_d tot_d = fum_d + wid_d;

      // see above
      //
      if ( (fum_d >> met_g != fum_w) || (tot_d  - wid_d != fum_d) ) {
        u3m_bail(c3__fail);
        return;
      }
      else if ( fum_d > len_d ) {
        return;
      }

      if ( tot_d > len_d ) {
        wid_d -= tot_d - len_d;
      }

      src_w += fum_d >> u3a_word_bits_log;
      dst_w += tou_d >> u3a_word_bits_log;
      bif_g  = fum_d & (u3a_word_bits - 1);
      bit_g  = tou_d & (u3a_word_bits - 1);
    }

    u3r_chop_bits(bif_g, wid_d, bit_g, dst_w, src_w);
  }
}

/* u3r_chop():
**
**   Into the bloq space of `met`, from position `fum` for a
**   span of `wid`, to position `tou`, XOR from atom `src`
**   into `dst_w`.
**
**   NB: [dst_w] must have space for [tou_w + wid_w] bloqs
*/
void
u3r_chop(c3_g  met_g,
         c3_w  fum_w,
         c3_w  wid_w,
         c3_w  tou_w,
         c3_w* dst_w,
         u3_atom src)
{
  c3_w* src_w;
  c3_w  len_w;

  if ( _(u3a_is_cat(src)) ) {
    len_w = src ? 1 : 0;
    src_w = &src;
  }
  else {
    u3a_atom* src_u = u3a_to_ptr(src);

    u3_assert(u3_none != src);
    u3_assert(_(u3a_is_atom(src)));

    len_w = src_u->len_w;
    src_w = src_u->buf_w;
  }

  u3r_chop_words(met_g, fum_w, wid_w, tou_w, dst_w, len_w, src_w);
}

/* u3r_string(): `a` as malloced C string.
*/
c3_c*
u3r_string(u3_atom a)
{
  c3_w  met_w = u3r_met(3, a);
  c3_c* str_c = c3_malloc(met_w + 1);

  u3r_bytes(0, met_w, (c3_y*)str_c, a);
  str_c[met_w] = 0;
  return str_c;
}

/* u3r_tape(): `a`, a list of bytes, as malloced C string.
*/
c3_y*
u3r_tape(u3_noun a)
{
  u3_noun b;
  c3_w    i_w;
  c3_y    *a_y;

  for ( i_w = 0, b=a; c3y == u3a_is_cell(b); i_w++, b=u3a_t(b) )
    ;
  a_y = c3_malloc(i_w + 1);

  for ( i_w = 0, b=a; c3y == u3a_is_cell(b); i_w++, b=u3a_t(b) ) {
    a_y[i_w] = u3a_h(b);
  }
  a_y[i_w] = 0;

  return a_y;
}

/* u3r_mug_both(): Join two mugs.
*/
c3_m
u3r_mug_both(c3_m lef_l, c3_m rit_l)
{
  c3_y len_y = 4 + ((c3_bits_half(rit_l) + 0x7) >> 3);
  c3_h syd_h = 0xdeadbeef;
  c3_h   i_h = 0;
  c3_y buf_y[8];

  buf_y[0] = lef_l         & 0xff;
  buf_y[1] = (lef_l >>  8) & 0xff;
  buf_y[2] = (lef_l >> 16) & 0xff;
  buf_y[3] = (lef_l >> 24) & 0xff;
  buf_y[4] = rit_l         & 0xff;
  buf_y[5] = (rit_l >>  8) & 0xff;
  buf_y[6] = (rit_l >> 16) & 0xff;
  buf_y[7] = (rit_l >> 24) & 0xff;

  while ( i_h < 8 ) {
    c3_h haz_h;
    c3_m ham_m;

    MurmurHash3_x86_32(buf_y, len_y, syd_h, &haz_h);
    ham_m = (haz_h >> 31) ^ (haz_h & 0x7fffffff);

    if ( 0 == ham_m ) {
      syd_h++; i_h++;
    }
    else {
      return ham_m;
    }
  }

  return 0xfffe;
}

/* u3r_mug_bytes(): Compute the mug of `buf`, `len`, LSW first.
*/
c3_m
u3r_mug_bytes(const c3_y *buf_y,
              c3_h        len_h)
{
  c3_h syd_h = 0xcafebabe;
  c3_h   i_h = 0;

  while ( i_h < 8 ) {
    c3_h haz_h;
    c3_m ham_m;

    MurmurHash3_x86_32(buf_y, len_h, syd_h, &haz_h);
    ham_m = (haz_h >> 31) ^ (haz_h & 0x7fffffff);

    if ( 0 == ham_m ) {
      syd_h++; i_h++;
    }
    else {
      return ham_m;
    }
  }

  return 0x7fff;
}

/* u3r_mug_c(): Compute the mug of `a`, LSB first.
*/
c3_m
u3r_mug_c(const c3_c* a_c)
{
  return u3r_mug_bytes((c3_y*)a_c, strlen(a_c));
}

/* u3r_mug_cell(): Compute the mug of the cell `[hed tel]`.
*/
c3_m
u3r_mug_cell(u3_noun hed,
             u3_noun tel)
{
  c3_h lus_h = u3r_mug(hed);
  c3_h biq_h = u3r_mug(tel);

  return u3r_mug_both(lus_h, biq_h);
}

/* u3r_mug_chub(): Compute the mug of `num`, LSW first.
*/
c3_m
u3r_mug_chub(c3_d num_d)
{
  return u3r_mug_chubs(&num_d, 1);
}

/* u3r_mug_words(): 31-bit nonzero MurmurHash3 on raw words.
*/
c3_m
u3r_mug_halfs(const c3_h* key_h, c3_w len_w)
{
  c3_h byt_h;

  //  ignore trailing zeros
  //
  while ( len_w && !key_h[len_w - 1] ) {
    len_w--;
  }

  //  calculate byte-width a la u3r_met(3, ...)
  //
  if ( !len_w ) {
    byt_h = 0;
  }
  else {
    c3_h gal_h = len_w - 1;
    c3_h daz_h = key_h[gal_h];

    byt_h = (gal_h << 2) + ((c3_bits_half(daz_h) + 7) >> 3);
  }

  //  XX: assumes little-endian
  //
  return u3r_mug_bytes((c3_y*)key_h, byt_h);
}

/* u3r_mug_chubs(): 31-bit nonzero MurmurHash3 on raw chubs.
*/
c3_m
u3r_mug_chubs(const c3_d* key_d, c3_w len_w)
{
  c3_d byt_w;

  //  ignore trailing zeros
  //
  while ( len_w && !key_d[len_w - 1] ) {
    len_w--;
  }

  //  calculate byte-width a la u3r_met(3, ...)
  //
  if ( !len_w ) {
    byt_w = 0;
  }
  else {
    c3_d gal_w = len_w - 1;
    c3_d daz_w = key_d[gal_w];

    byt_w = (gal_w << 3) + ((c3_bits_chub(daz_w) + 7) >> 3);
  }

  //  XX: assumes little-endian
  //
  return u3r_mug_bytes((c3_y*)key_d, byt_w);
}

/* u3r_mug_words(): 31-bit nonzero MurmurHash3 on raw words.
*/
c3_m
u3r_mug_words(const c3_w* key_w, c3_w len_w)
{
#ifndef VERE64
  return u3r_mug_halfs(key_w, len_w);
#else
  return u3r_mug_chubs(key_w, len_w);
#endif
}

/* u3r_mug_word(): 31-bit nonzero MurmurHash3 of a single word.
*/
c3_l
u3r_mug_word(c3_w key_w)
{
  return u3r_mug_words(&key_w, 1);
}


/* _cr_mug: stack frame for recording cell traversal
**          !mug == head-frame
*/
typedef struct {
  c3_h  mug_h;
  u3_cell cel;
} _cr_mugf;

/* _cr_mug_next(): advance mug calculation, pushing cells onto the stack.
*/
static inline c3_h
_cr_mug_next(u3a_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  veb is a direct atom, mug is not memoized
    //
    if ( c3y == u3a_is_cat(veb) ) {
      return (c3_h)u3r_mug_words(&veb, 1);
    }
    //  veb is indirect, a pointer into the loom
    //
    else {
      u3a_noun* veb_u = u3a_to_ptr(veb);

      //  veb has already been mugged, return memoized value
      //
      //    XX add debug assertion that mug is 31-bit?
      //
      if ( veb_u->mug_w ) {
        return (c3_h)veb_u->mug_w;
      }
      //  veb is an indirect atom, mug its bytes and memoize
      //
      else if ( c3y == u3a_is_atom(veb) ) {
        u3a_atom* vat_u = (u3a_atom*)veb_u;
        c3_h      mug_h = u3r_mug_words(vat_u->buf_w, vat_u->len_w);
        vat_u->mug_w = mug_h;
        return mug_h;
      }
      //  veb is a cell, push a stack frame to mark head-recursion
      //  and read the head
      //
      else {
        u3a_cell* cel_u = (u3a_cell*)veb_u;
        _cr_mugf* fam_u = u3a_push(pil_u);

        fam_u->mug_h = 0;
        fam_u->cel   = veb;

        veb = cel_u->hed;
        continue;
      }
    }
  }
}

/* u3r_mug(): statefully mug a noun with 31-bit murmur3.
*/
c3_h
u3r_mug(u3_noun veb)
{
  u3a_pile  pil_u;
  _cr_mugf* fam_u;
  c3_h      mug_h;

  //  sanity check
  //
  u3_assert( u3_none != veb );

  u3a_pile_prep(&pil_u, sizeof(*fam_u), __alignof__(*fam_u));

  //  commence mugging
  //
  mug_h = _cr_mug_next(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash mug and continue into the tail
      //
      if ( !fam_u->mug_h ) {
        u3a_cell* cel_u = u3a_to_ptr(fam_u->cel);

        fam_u->mug_h = mug_h;
        mug_h        = _cr_mug_next(&pil_u, cel_u->tel);
        fam_u        = u3a_peek(&pil_u);
      }
      //  tail-frame: calculate/memoize cell mug and pop the stack
      //
      else {
        u3a_cell* cel_u = u3a_to_ptr(fam_u->cel);

        mug_h        = u3r_mug_both(fam_u->mug_h, mug_h);
        cel_u->mug_w = mug_h;
        fam_u        = u3a_pop(&pil_u);
      }
    }
    while ( c3n == u3a_pile_done(&pil_u) );
  }

  return mug_h;
}

/* u3r_skip():
**
**  Extract a constant from a formula, ignoring
**  safe/static hints, doing no computation.
*/
u3_weak
u3r_skip(u3_noun fol)
{
  while ( c3y == u3du(fol) ) {
    switch ( u3h(fol) ) {
      default: return u3_none;
      case 1:  return u3t(fol);
      case 11: {
        u3_noun arg = u3t(fol),
                hod = u3h(arg);

        if ( (c3y == u3du(hod)) && (u3_none == u3r_skip(u3t(hod))) ) {
          return u3_none;
        }
        fol = u3t(arg);
      }
    }
  }
  return u3_none;
}

/* u3r_safe():
**
**  Returns yes if the formula won't crash
**  and has no hints, returning constant result
**  if possible. *out is undefined if the return
**  is c3n
*/
c3_o
u3r_safe(u3_noun fol, u3_weak* out)
{
  u3_noun h_fol, t_fol;
  c3_o saf_o;

  if ( c3n == u3r_cell(fol, &h_fol, &t_fol) ) {
    return c3n;
  }
  switch ( h_fol ) {
    default: return c3n;
    case 0:
      *out = u3_none;
      return __(1 == t_fol);
      
    case 1:
      *out = t_fol;
      return c3y;

    case 3: {
      u3_weak o;
      saf_o = u3r_safe(t_fol, &o);
      if ( _(saf_o) ) {
        *out = (u3_none == o) ? u3_none : u3du(o);
      }
      return saf_o;
    }

    case 5: {
      u3_noun p, q;
      u3_weak o1, o2;
      saf_o = c3a(u3r_cell(t_fol, &p, &q),
              c3a(u3r_safe(p, &o1), u3r_safe(q, &o2)));

      if ( _(saf_o) ) {
        *out = (u3_none == o1) ? u3_none
             : (u3_none == o2) ? u3_none
             : u3r_sing(o1, o2);
      }
      return saf_o;
    }

    case 6: {
      u3_noun p, q, r;
      u3_weak o;
      saf_o = c3a(u3r_trel(t_fol, &p, &q, &r), u3r_safe(p, &o));

      if ( _(saf_o) ) {
        switch ( o ) {
          case c3y:  return u3r_safe(q, out);
          case c3n:  return u3r_safe(r, out);
          default:   return c3n;
        }
      }
      else {
        return c3n;
      }
    }

    case 7:
    case 8: {
      u3_noun p, q;
      u3_weak o;
      return c3a(u3r_cell(t_fol, &p, &q),
             c3a(u3r_safe(p, &o), u3r_safe(q, out)));
    }
  }
}

/* u3r_word_buffer(): returns word buffer pointer of atom `*a`
** and the length of the buffer
*/
c3_w*
u3r_word_buffer(u3_atom* a, c3_w* len_w)
{
  if ( _(u3a_is_cat(*a)) ) {
    *len_w = 1;
    return a;
  }
  u3a_atom* pug_u = u3a_to_ptr(*a);
  *len_w = pug_u->len_w;
  return pug_u->buf_w;
}

static inline c3_ys
_comp_words(c3_w a_w, c3_w b_w)
{
  return (c3_ys)(a_w > b_w) - (c3_ys)(a_w < b_w);
}

/* u3r_comp(): compares two atoms:
** returns 1 if a > b, -1 if a < b, 0 if they are equal
*/
c3_ys
u3r_comp(u3_atom a, u3_atom b)
{
  if (c3y == u3a_is_cat(a) || c3y == u3a_is_cat(b)) {
    return _comp_words(a, b);
  }
  
  if ( a == b ) return 0;

  u3a_atom* a_u = u3a_to_ptr(a);
  u3a_atom* b_u = u3a_to_ptr(b);

  if (a_u->len_w != b_u->len_w) {
    return _comp_words(a_u->len_w, b_u->len_w);
  }

  c3_w* a_w = a_u->buf_w;
  c3_w* b_w = b_u->buf_w;

  for (c3_w i_w = a_u->len_w; i_w--;) {
    if ( a_w[i_w] != b_w[i_w] ) {
      return _comp_words(a_w[i_w], b_w[i_w]);
    }
  }

  return 0;
}
