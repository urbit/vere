/// @file
///
/// A compact open-addressing table for the home-road hashcons set.
///
/// A bucket holds a member's slot index (its loom offset in cell-sized
/// units, plus 1; 0 is empty) and its probe distance: five bytes, 12
/// buckets to a 64-byte line, at either width. Linear probing
/// in Robin Hood order, with backward-shift deletion: no tombstones,
/// and neither a lookup nor an erase reads another member's box except
/// to compare one with the same home bucket. The hash is the mug, so a
/// lookup by value needs no noun; a resize reads each member's mug from
/// its box. Sizes aren't powers of two (multiply-shift).
///
/// Keys are offsets and the table is flat, so it could live on the loom.
///
/// NB: static functions, for manage.c only.

#ifndef U3_SOLETAB_H
#define U3_SOLETAB_H

#include "c3/c3.h"
#include "allocate.h"
#include "retrieve.h"

#define _CS_DIS_MAX  254    //  largest distance a bucket holds
#define _CS_LIN_MIN  64     //  fewest lines

/* a key is a member's slot index + 1, or 0 for an empty bucket: its
** loom offset in units of a cell's size. every noun box is at least a
** cell's size, a power of two, naturally aligned, so this loses nothing;
** members are home nouns, and the home road's heap is low (it's north),
** so 4 bytes cover the whole 32-bit loom and 128 GiB of home heap at 64
** bits. each bucket also holds its distance from its home bucket.
*/
#define _CS_PER      12     //  buckets a line
#define _CS_UNIT     ((c3_d)c3_wiseof(u3a_cell))  //  words a key step
#define _CS_KEY_MAX  0xffffffffULL

typedef struct {
  c3_h key_h[_CS_PER];      //  key
  c3_y dis_y[_CS_PER];      //  distance from the home bucket
  c3_y pad_y[4];
} _cs_line;

_Static_assert(64 == sizeof(_cs_line), "soletab: line size");

//  a key is a noun's offset in cell-sized steps: every noun box is a step
//  or more, in a power-of-two size class, naturally aligned (palloc.c), so
//  its offset is a multiple of the step
//
_Static_assert(0 == (c3_wiseof(u3a_cell) & (c3_wiseof(u3a_cell) - 1)),
               "soletab: key step is a power of two");
_Static_assert((2 * c3_wiseof(u3a_atom)) > c3_wiseof(u3a_cell),
               "soletab: an atom's box is a key step or more");

typedef struct {
  _cs_line* lin_u;          //  lines
  c3_d      lin_d;          //  number of lines
  c3_d      cap_d;          //  buckets, _CS_PER * lin_d
  c3_d      len_d;          //  members
  c3_d      flo_d;          //  fewest lines a rebuild may use, or 0
  c3_y      dmx_y;          //  no distance here is larger
  c3_t      cel_t;          //  cells, else atoms
} _cs_tab;

/* meters, for both tables (U3_MEASURE).
*/
static struct {
  c3_d ste_d;               //  buckets visited by lookups
  c3_d der_d;               //  members compared (boxes read)
  c3_d gro_d[2];            //  resizes: between events, at the limit
  c3_d gus_d;               //  time resizing, microseconds
  c3_d dmx_d;               //  largest distance placed
} _cs_met;

/* load factors, in percent, from replays of one ship: between events,
** grow above _CS_GRO and shrink below _CS_SHR; within an event, grow at
** the limit _CS_LIM; size to _CS_TAR on a resize.
*/
#define _CS_GRO  90
#define _CS_LIM  95
#define _CS_TAR  85
#define _CS_SHR  75

/* bulk filling (a rebuild): grow by twice at 70%, keeping inserts at low
** load, and settle to the target once done (_cs_tend()).
*/
static c3_t _cs_bul_t;

/* _cs_key(): the key in bucket [i_d].
*/
static inline c3_h
_cs_key(_cs_tab* tab_u, c3_d i_d)
{
  return tab_u->lin_u[i_d / _CS_PER].key_h[i_d % _CS_PER];
}

/* _cs_key_put(): put [key_h] in bucket [i_d].
*/
static inline void
_cs_key_put(_cs_tab* tab_u, c3_d i_d, c3_h key_h)
{
  tab_u->lin_u[i_d / _CS_PER].key_h[i_d % _CS_PER] = key_h;
}

static inline c3_y*
_cs_dis(_cs_tab* tab_u, c3_d i_d)
{
  return &tab_u->lin_u[i_d / _CS_PER].dis_y[i_d % _CS_PER];
}

static inline c3_d
_cs_next(_cs_tab* tab_u, c3_d i_d)
{
  return ( ++i_d == tab_u->cap_d ) ? 0 : i_d;
}

/* _cs_reaches(): [som] has a key. a home noun past the keys' reach (128
**   GiB of home heap at 64 bits) stays out of the set, unshared: no ship
**   is near that size, and the startup warns (see u3m_sole_ream()).
*/
static inline c3_t
_cs_reaches(u3_noun som)
{
  return (((c3_d)u3a_to_off(som) / _CS_UNIT) + 1) <= _CS_KEY_MAX;
}

/* _cs_slot(): [som]'s key; see _cs_reaches().
*/
static inline c3_h
_cs_slot(u3_noun som)
{
  return (c3_h)(((c3_d)u3a_to_off(som) / _CS_UNIT) + 1);
}

/* _cs_noun(): the member whose key is [key_h].
*/
static inline u3_noun
_cs_noun(_cs_tab* tab_u, c3_h key_h)
{
  c3_w off_w = (c3_w)((c3_d)(key_h - 1) * _CS_UNIT);
  return ( tab_u->cel_t ) ? u3a_to_pom(off_w) : u3a_to_pug(off_w);
}

/* _cs_home(): the home bucket of mug [mug_h].
*/
static inline c3_d
_cs_home(_cs_tab* tab_u, c3_h mug_h)
{
  c3_d has_d = (c3_d)mug_h * 11400714819323198485ULL;

  //  the high half of the product, at any number of buckets
  //
  return (c3_d)(((c3_q)has_d * tab_u->cap_d) >> 64);
}

/* _cs_reach(): words of home heap the keys cover.
*/
static inline c3_d
_cs_reach(void)
{
  return _CS_KEY_MAX * _CS_UNIT;
}

/* _cs_alloc(): an empty table of [lin_d] lines.
*/
static void
_cs_alloc(_cs_tab* tab_u, c3_d lin_d)
{
  lin_d = c3_max(lin_d, _CS_LIN_MIN);
  tab_u->lin_u = c3_calloc(lin_d * sizeof(_cs_line));
  tab_u->lin_d = lin_d;
  tab_u->cap_d = lin_d * _CS_PER;
  tab_u->len_d = 0;
  tab_u->dmx_y = 0;
}

static void
_cs_init(_cs_tab* tab_u, c3_t cel_t)
{
  tab_u->cel_t = cel_t;
  tab_u->flo_d = 0;
  _cs_alloc(tab_u, _CS_LIN_MIN);
}

/* _cs_lines_for(): lines to hold [len_d] at [pct_w] percent.
*/
static inline c3_d
_cs_lines_for(c3_d len_d, c3_w pct_w)
{
  c3_d cap_d = (len_d * 100) / pct_w + 1;
  return (cap_d + _CS_PER - 1) / _CS_PER;
}

/* _cs_fits(): [key_h] can go in from bucket [i_d] at distance [dis_d],
**   as _cs_place_from() would put it, with every distance still within
**   _CS_DIS_MAX. a bucket holds no more, and members with one mug share
**   a home bucket, so the 256th of them can't fit at any size.
**
**   placing a member raises the table's largest distance by at most one,
**   so only a table already at the limit needs the walk.
*/
static c3_t
_cs_fits_walk(_cs_tab* tab_u, c3_d i_d, c3_d dis_d)
{
  while ( 1 ) {
    c3_y dis_y = *_cs_dis(tab_u, i_d);

    if ( dis_d > _CS_DIS_MAX )   return 0;
    if ( !_cs_key(tab_u, i_d) )  return 1;
    if ( dis_y < dis_d )         dis_d = dis_y;

    i_d = _cs_next(tab_u, i_d);
    dis_d++;
  }
}

static inline c3_t
_cs_fits(_cs_tab* tab_u, c3_d i_d, c3_d dis_d)
{
  return (tab_u->dmx_y < _CS_DIS_MAX) || _cs_fits_walk(tab_u, i_d, dis_d);
}

/* _cs_place_from(): put [key_h], known absent and known to fit
**   (_cs_fits()), Robin Hood, from bucket [i_d] at distance [dis_d] from
**   its home; produces its bucket.
*/
static c3_d
_cs_place_from(_cs_tab* tab_u, c3_h key_h, c3_d i_d, c3_d dis_d)
{
  c3_d pos_d = ~0ULL;

  while ( 1 ) {
    c3_h  kei_h = _cs_key(tab_u, i_d);
    c3_y* dis_p = _cs_dis(tab_u, i_d);

    //  an empty bucket, or a member nearer its home, which moves on
    //
    if ( !kei_h || (*dis_p < dis_d) ) {
      c3_y kis_y = *dis_p;

      _cs_key_put(tab_u, i_d, key_h);
      *dis_p = (c3_y)dis_d;
      if ( dis_d > _cs_met.dmx_d ) _cs_met.dmx_d = dis_d;
      if ( dis_d > tab_u->dmx_y )  tab_u->dmx_y  = (c3_y)dis_d;
      if ( ~0ULL == pos_d ) pos_d = i_d;

      if ( !kei_h ) {
        tab_u->len_d++;
        return pos_d;
      }
      key_h = kei_h;
      dis_d = kis_y;
    }
    i_d = _cs_next(tab_u, i_d);
    if ( ++dis_d > _CS_DIS_MAX ) {
      //  the caller checked (_cs_fits())
      //
      u3_assert(!"soletab: distance overflow");
    }
  }
}

/* _cs_fill(): make [tab_u] an empty table of [lin_d] lines and put
**   [old_u]'s members in it, reading each one's mug; 0 if one doesn't
**   fit.
*/
static c3_t
_cs_fill(_cs_tab* tab_u, _cs_tab* old_u, c3_d lin_d)
{
  _cs_alloc(tab_u, lin_d);

  for ( c3_d l_d = 0; l_d < old_u->lin_d; l_d++ ) {
    _cs_line* lin_u = &old_u->lin_u[l_d];

    //  a few lines ahead, fetch the boxes whose mugs we'll read
    //
    if ( l_d + 4 < old_u->lin_d ) {
      _cs_line* pre_u = &old_u->lin_u[l_d + 4];
      for ( c3_w j_w = 0; j_w < _CS_PER; j_w++ ) {
        c3_h key_h = pre_u->key_h[j_w];
        if ( key_h ) {
          __builtin_prefetch(u3a_to_ptr(_cs_noun(old_u, key_h)));
        }
      }
    }

    for ( c3_w j_w = 0; j_w < _CS_PER; j_w++ ) {
      c3_h key_h = lin_u->key_h[j_w];

      if ( key_h ) {
        c3_d i_d = _cs_home(tab_u, u3r_mug(_cs_noun(old_u, key_h)));

        if ( !_cs_fits(tab_u, i_d, 0) ) {
          return 0;
        }
        _cs_place_from(tab_u, key_h, i_d, 0);
      }
    }
  }
  return 1;
}

/* _cs_resize(): rebuild into [lin_d] lines, or more.
**
**   every member stays a member (the children of a member are members),
**   so if one doesn't fit, the rebuild starts over at twice the size: a
**   smaller table packs neighboring home buckets closer, and a run that
**   fit before can pass the largest distance. that size becomes the
**   table's floor, so the next shrink doesn't try again.
*/
#define _CS_TRY_MAX  8

static void
_cs_resize(_cs_tab* tab_u, c3_d lin_d)
{
  _cs_tab old_u = *tab_u;
  c3_w    try_w = 0;

  lin_d = c3_max(lin_d, tab_u->flo_d);

  while ( !_cs_fill(tab_u, &old_u, lin_d) ) {
    c3_free(tab_u->lin_u);
    u3_assert( ++try_w <= _CS_TRY_MAX );
    lin_d = tab_u->lin_d * 2;
    tab_u->flo_d = lin_d;
  }

  c3_free(old_u.lin_u);
}

static void
_cs_resize_timed(_cs_tab* tab_u, c3_d lin_d, c3_w why_w)
{
  struct timeval bef_u, aft_u;

  gettimeofday(&bef_u, 0);
  _cs_resize(tab_u, lin_d);
  gettimeofday(&aft_u, 0);

  _cs_met.gro_d[why_w]++;
  _cs_met.gus_d += (c3_d)(aft_u.tv_sec - bef_u.tv_sec) * 1000000
                 + (aft_u.tv_usec - bef_u.tv_usec);
}

/* _cs_seek(): walk from the home bucket of [mug_h] to the member for
**   which [pre_f] holds, if there is one. [*pos_d] is its bucket, or else
**   the bucket where such a member would go, at distance [*dis_d].
*/
static inline c3_t
_cs_seek(_cs_tab* tab_u,
         c3_h     mug_h,
         c3_t   (*pre_f)(u3_noun, void*),
         void*    ptr_v,
         c3_d*    pos_d,
         c3_d*    dis_d)
{
  c3_d   i_d = _cs_home(tab_u, mug_h);
  c3_d   d_d = 0;
  c3_t fon_t = 0;

  while ( 1 ) {
    c3_y dis_y = *_cs_dis(tab_u, i_d);
    c3_h key_h;

    _cs_met.ste_d++;

    //  past where it would be, in Robin Hood order (an empty bucket has
    //  distance 0, so only distance 0 needs the key)
    //
    if ( (dis_y < d_d) || !(key_h = _cs_key(tab_u, i_d)) ) {
      break;
    }

    //  the same home bucket: compare
    //
    if ( dis_y == d_d ) {
      _cs_met.der_d++;
      if ( pre_f(_cs_noun(tab_u, key_h), ptr_v) ) {
        fon_t = 1;
        break;
      }
    }

    i_d = _cs_next(tab_u, i_d);
    d_d++;
  }

  *pos_d = i_d;
  *dis_d = d_d;
  return fon_t;
}

/* _cs_get(): the member found by [pre_f], or none.
*/
static inline u3_weak
_cs_get(_cs_tab* tab_u, c3_h mug_h, c3_t (*pre_f)(u3_noun, void*), void* ptr_v)
{
  c3_d i_d, dis_d;

  return _cs_seek(tab_u, mug_h, pre_f, ptr_v, &i_d, &dis_d)
         ? _cs_noun(tab_u, _cs_key(tab_u, i_d))
         : u3_none;
}

/* _cs_room(): before an insert, at the limit, grow: by about twice, to
**   leave room for what's still coming (a rebuild, or a large exit).
*/
static inline void
_cs_room(_cs_tab* tab_u)
{
  if ( c3_unlikely(_cs_bul_t) ) {
    if ( (tab_u->len_d + 1) * 100 > tab_u->cap_d * 70 ) {
      _cs_resize_timed(tab_u, _cs_lines_for(tab_u->len_d * 2 + 1, 70), 1);
    }
  }
  else if ( (tab_u->len_d + 1) * 100 > tab_u->cap_d * _CS_LIM ) {
    _cs_resize_timed(tab_u, _cs_lines_for((tab_u->len_d * 3) / 2 + 1, _CS_TAR), 1);
  }
}

/* _cs_add(): add [som], known absent; produces its bucket, or ~0 if
**   it doesn't fit (_cs_fits()), and stays out.
*/
static inline c3_d
_cs_add(_cs_tab* tab_u, u3_noun som)
{
  c3_d i_d;

  _cs_room(tab_u);
  i_d = _cs_home(tab_u, u3r_mug(som));

  return _cs_fits(tab_u, i_d, 0)
         ? _cs_place_from(tab_u, _cs_slot(som), i_d, 0)
         : ~0ULL;
}

/* _cs_put(): the member found by [pre_f] for [som] of mug [mug_h], or
**   else add [som], in one walk: a miss stops where [som] goes. produces
**   the member, and its bucket in [*pos_d] if added (else ~0); or none,
**   if [som] has no equal and doesn't fit (_cs_fits()), and stays out.
*/
static inline u3_noun
_cs_put(_cs_tab* tab_u, u3_noun som, c3_h mug_h,
        c3_t (*pre_f)(u3_noun, void*), c3_d* pos_d)
{
  c3_d i_d, dis_d;

  _cs_room(tab_u);

  if ( _cs_seek(tab_u, mug_h, pre_f, &som, &i_d, &dis_d) ) {
    *pos_d = ~0ULL;
    return _cs_noun(tab_u, _cs_key(tab_u, i_d));
  }
  if ( !_cs_fits(tab_u, i_d, dis_d) ) {
    *pos_d = ~0ULL;
    return u3_none;
  }
  *pos_d = _cs_place_from(tab_u, _cs_slot(som), i_d, dis_d);
  return som;
}

/* _cs_erase(): remove member [som], if present; produces its bucket,
**   or ~0. backward shift: the run after it moves up one.
*/
static c3_d
_cs_erase(_cs_tab* tab_u, u3_noun som)
{
  c3_h key_h = _cs_slot(som);
  c3_d   i_d = _cs_home(tab_u, u3r_mug(som));
  c3_d dis_d = 0;
  c3_d pos_d;

  while ( 1 ) {
    c3_h kei_h = _cs_key(tab_u, i_d);
    c3_y dis_y = *_cs_dis(tab_u, i_d);

    if ( !kei_h || (dis_y < dis_d) ) return ~0ULL;
    if ( kei_h == key_h ) break;
    i_d = _cs_next(tab_u, i_d);
    dis_d++;
  }

  pos_d = i_d;

  //  an empty bucket has distance 0, so a distance of 0 ends the run
  //
  while ( 1 ) {
    c3_d   j_d = _cs_next(tab_u, i_d);
    c3_y dij_y = *_cs_dis(tab_u, j_d);

    if ( !dij_y ) {
      _cs_key_put(tab_u, i_d, 0);
      *_cs_dis(tab_u, i_d) = 0;
      break;
    }
    _cs_key_put(tab_u, i_d, _cs_key(tab_u, j_d));
    *_cs_dis(tab_u, i_d) = dij_y - 1;
    i_d = j_d;
  }

  tab_u->len_d--;
  return pos_d;
}

/* _cs_tend(): between events, grow above _CS_GRO, or shrink below
**   _CS_SHR.
*/
static void
_cs_tend(_cs_tab* tab_u)
{
  c3_d pct_d = ( tab_u->len_d * 100 ) / tab_u->cap_d;
  if (  (pct_d >= _CS_GRO)
     || ((pct_d < _CS_SHR) && (tab_u->lin_d > _CS_LIN_MIN)) )
  {
    c3_d lin_d = c3_max(_cs_lines_for(tab_u->len_d, _CS_TAR), tab_u->flo_d);

    if ( lin_d != tab_u->lin_d ) {
      _cs_resize_timed(tab_u, lin_d, 0);
    }
  }
}

/* _cs_reserve(): room for [len_d] at the target load.
*/
static void
_cs_reserve(_cs_tab* tab_u, c3_d len_d)
{
  c3_d lin_d = _cs_lines_for(len_d, _CS_TAR);

  if ( lin_d > tab_u->lin_d ) {
    _cs_resize(tab_u, lin_d);
  }
}

/* _cs_clear(): remove everything, calling [fun_f] on each member first.
*/
static void
_cs_clear(_cs_tab* tab_u, void (*fun_f)(u3_noun))
{
  for ( c3_d l_d = 0; l_d < tab_u->lin_d; l_d++ ) {
    _cs_line* lin_u = &tab_u->lin_u[l_d];

    for ( c3_w j_w = 0; j_w < _CS_PER; j_w++ ) {
      c3_h key_h = lin_u->key_h[j_w];
      if ( key_h ) {
        fun_f(_cs_noun(tab_u, key_h));
      }
    }
  }
  memset(tab_u->lin_u, 0, tab_u->lin_d * sizeof(_cs_line));
  tab_u->len_d = 0;
  tab_u->dmx_y = 0;
  tab_u->flo_d = 0;
}

/* _cs_bytes(): memory held.
*/
static inline c3_d
_cs_bytes(_cs_tab* tab_u)
{
  return tab_u->lin_d * sizeof(_cs_line);
}

#endif /* ifndef U3_SOLETAB_H */
