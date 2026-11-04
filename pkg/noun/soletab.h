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

typedef c3_h _cs_key_t;

typedef struct {
  c3_h key_h[_CS_PER];      //  key
  c3_y dis_y[_CS_PER];      //  distance from the home bucket
  c3_y pad_y[4];
} _cs_line;

_Static_assert(64 == sizeof(_cs_line), "soletab: line size");

typedef struct {
  _cs_line* lin_u;          //  lines
  c3_d      lin_d;          //  number of lines
  c3_d      cap_d;          //  buckets, _CS_PER * lin_d
  c3_d      len_d;          //  members
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

/* _cs_line_key(): the key in bucket [j_w] of line [lin_u].
*/
static inline _cs_key_t
_cs_line_key(_cs_line* lin_u, c3_w j_w)
{
  return lin_u->key_h[j_w];
}

/* _cs_key(): the key in bucket [i_d].
*/
static inline _cs_key_t
_cs_key(_cs_tab* tab_u, c3_d i_d)
{
  return _cs_line_key(&tab_u->lin_u[i_d / _CS_PER], i_d % _CS_PER);
}

/* _cs_key_put(): put [key] in bucket [i_d].
*/
static inline void
_cs_key_put(_cs_tab* tab_u, c3_d i_d, _cs_key_t key)
{
  tab_u->lin_u[i_d / _CS_PER].key_h[i_d % _CS_PER] = key;
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

/* _cs_slot(): [som]'s key. a home noun past the keys' reach (128 GiB of
**   home heap at 64 bits) can't be a member: no ship is near that size,
**   and the startup warns long before (see u3m_sole_ream()).
*/
static inline _cs_key_t
_cs_slot(u3_noun som)
{
  c3_d key_d = ((c3_d)u3a_to_off(som) / _CS_UNIT) + 1;

  if ( c3_unlikely(key_d > _CS_KEY_MAX) ) {
    u3_assert(!"hashcons: a home noun past the set's keys");
  }
  return (_cs_key_t)key_d;
}

/* _cs_noun(): the member whose key is [key].
*/
static inline u3_noun
_cs_noun(_cs_tab* tab_u, _cs_key_t key)
{
  c3_w off_w = (c3_w)((c3_d)(key - 1) * _CS_UNIT);
  return ( tab_u->cel_t ) ? u3a_to_pom(off_w) : u3a_to_pug(off_w);
}

/* _cs_home(): the home bucket of mug [mug_h].
*/
static inline c3_d
_cs_home(_cs_tab* tab_u, c3_h mug_h)
{
  c3_d has_d = (c3_d)mug_h * 11400714819323198485ULL;
  return ((has_d >> 32) * tab_u->cap_d) >> 32;
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
}

static void
_cs_init(_cs_tab* tab_u, c3_t cel_t)
{
  tab_u->cel_t = cel_t;
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

/* _cs_place_from(): put [key], known absent, Robin Hood, from bucket
**   [i_d] at distance [dis_d] from its home; produces its bucket.
*/
static c3_d
_cs_place_from(_cs_tab* tab_u, _cs_key_t key, c3_d i_d, c3_d dis_d)
{
  c3_d pos_d = ~0ULL;

  while ( 1 ) {
    _cs_key_t kei = _cs_key(tab_u, i_d);
    c3_y*     dis_p = _cs_dis(tab_u, i_d);

    if ( !kei ) {
      _cs_key_put(tab_u, i_d, key);
      *dis_p = (c3_y)dis_d;
      if ( dis_d > _cs_met.dmx_d ) _cs_met.dmx_d = dis_d;
      tab_u->len_d++;
      return ( ~0ULL == pos_d ) ? i_d : pos_d;
    }
    if ( *dis_p < dis_d ) {
      c3_y kis_y = *dis_p;

      _cs_key_put(tab_u, i_d, key);
      *dis_p = (c3_y)dis_d;
      if ( dis_d > _cs_met.dmx_d ) _cs_met.dmx_d = dis_d;
      if ( ~0ULL == pos_d ) pos_d = i_d;
      key   = kei;
      dis_d = kis_y;
    }
    i_d = _cs_next(tab_u, i_d);
    if ( ++dis_d > _CS_DIS_MAX ) {
      //  can't happen below the limit: a distance of 255 at 92% load
      //
      u3_assert(!"soletab: distance overflow");
    }
  }
}

/* _cs_place(): put [key] of mug [mug_h], known absent.
*/
static inline c3_d
_cs_place(_cs_tab* tab_u, _cs_key_t key, c3_h mug_h)
{
  return _cs_place_from(tab_u, key, _cs_home(tab_u, mug_h), 0);
}

/* _cs_resize(): rebuild into [lin_d] lines, reading each member's mug.
*/
static void
_cs_resize(_cs_tab* tab_u, c3_d lin_d)
{
  _cs_tab old_u = *tab_u;

  _cs_alloc(tab_u, lin_d);

  for ( c3_d l_d = 0; l_d < old_u.lin_d; l_d++ ) {
    _cs_line* lin_u = &old_u.lin_u[l_d];

    //  a few lines ahead, fetch the boxes whose mugs we'll read
    //
    if ( l_d + 4 < old_u.lin_d ) {
      _cs_line* pre_u = &old_u.lin_u[l_d + 4];
      for ( c3_w j_w = 0; j_w < _CS_PER; j_w++ ) {
        _cs_key_t key = _cs_line_key(pre_u, j_w);
        if ( key ) {
          __builtin_prefetch(u3a_to_ptr(_cs_noun(&old_u, key)));
        }
      }
    }

    for ( c3_w j_w = 0; j_w < _CS_PER; j_w++ ) {
      _cs_key_t key = _cs_line_key(lin_u, j_w);

      if ( key ) {
        u3_noun som = _cs_noun(&old_u, key);
        _cs_place(tab_u, key, u3r_mug(som));
      }
    }
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

/* _cs_find(): the bucket of the member of mug [mug_h] for which
**   [pre_f] holds, or ~0.
*/
static inline c3_d
_cs_find(_cs_tab* tab_u, c3_h mug_h, c3_t (*pre_f)(u3_noun, void*), void* ptr_v)
{
  c3_d i_d   = _cs_home(tab_u, mug_h);
  c3_d dis_d = 0;

  while ( 1 ) {
    c3_y dis_y = *_cs_dis(tab_u, i_d);
    _cs_key_t key;

    _cs_met.ste_d++;

    //  past where it would be, in Robin Hood order (an empty bucket has
    //  distance 0, so only distance 0 needs the key)
    //
    if ( (dis_y < dis_d) || !(key = _cs_key(tab_u, i_d)) ) {
      return ~0ULL;
    }

    //  the same home bucket: compare
    //
    if ( dis_y == dis_d ) {
      _cs_met.der_d++;
      if ( pre_f(_cs_noun(tab_u, key), ptr_v) ) {
        return i_d;
      }
    }

    i_d = _cs_next(tab_u, i_d);
    dis_d++;
  }
}

/* _cs_get(): the member found by [pre_f], or none.
*/
static inline u3_weak
_cs_get(_cs_tab* tab_u, c3_h mug_h, c3_t (*pre_f)(u3_noun, void*), void* ptr_v)
{
  c3_d i_d = _cs_find(tab_u, mug_h, pre_f, ptr_v);
  return ( ~0ULL == i_d ) ? u3_none : _cs_noun(tab_u, _cs_key(tab_u, i_d));
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

/* _cs_add(): add [som], known absent; produces its bucket.
*/
static inline c3_d
_cs_add(_cs_tab* tab_u, u3_noun som)
{
  _cs_room(tab_u);
  return _cs_place(tab_u, _cs_slot(som), u3r_mug(som));
}

/* _cs_put(): the member found by [pre_f] for [som] of mug [mug_h], or
**   else add [som], in one walk: a miss stops where [som] goes. produces
**   the member, and its bucket in [*pos_d] if added (else ~0).
*/
static inline u3_noun
_cs_put(_cs_tab* tab_u, u3_noun som, c3_h mug_h,
        c3_t (*pre_f)(u3_noun, void*), c3_d* pos_d)
{
  c3_d i_d, dis_d = 0;

  _cs_room(tab_u);
  i_d = _cs_home(tab_u, mug_h);
  *pos_d = ~0ULL;

  while ( 1 ) {
    c3_y dis_y = *_cs_dis(tab_u, i_d);
    _cs_key_t key;

    _cs_met.ste_d++;

    if ( (dis_y < dis_d) || !(key = _cs_key(tab_u, i_d)) ) {
      *pos_d = _cs_place_from(tab_u, _cs_slot(som), i_d, dis_d);
      return som;
    }
    if ( dis_y == dis_d ) {
      u3_noun can = _cs_noun(tab_u, key);

      _cs_met.der_d++;
      if ( pre_f(can, &som) ) {
        return can;
      }
    }
    i_d = _cs_next(tab_u, i_d);
    dis_d++;
  }
}

/* _cs_erase(): remove member [som], if present; produces its bucket,
**   or ~0. backward shift: the run after it moves up one.
*/
static c3_d
_cs_erase(_cs_tab* tab_u, u3_noun som)
{
  _cs_key_t key   = _cs_slot(som);
  c3_d      i_d   = _cs_home(tab_u, u3r_mug(som));
  c3_d      dis_d = 0;
  c3_d      pos_d;

  while ( 1 ) {
    _cs_key_t kei   = _cs_key(tab_u, i_d);
    c3_y      dis_y = *_cs_dis(tab_u, i_d);

    if ( !kei || (dis_y < dis_d) ) return ~0ULL;
    if ( kei == key ) break;
    i_d = _cs_next(tab_u, i_d);
    dis_d++;
  }

  pos_d = i_d;

  //  an empty bucket has distance 0, so a distance of 0 ends the run
  //
  while ( 1 ) {
    c3_d j_d   = _cs_next(tab_u, i_d);
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
    c3_d lin_d = _cs_lines_for(tab_u->len_d, _CS_TAR);

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
      _cs_key_t key = _cs_line_key(lin_u, j_w);
      if ( key ) {
        fun_f(_cs_noun(tab_u, key));
      }
    }
  }
  memset(tab_u->lin_u, 0, tab_u->lin_d * sizeof(_cs_line));
  tab_u->len_d = 0;
}

/* _cs_bytes(): memory held.
*/
static inline c3_d
_cs_bytes(_cs_tab* tab_u)
{
  return tab_u->lin_d * sizeof(_cs_line);
}

#endif /* ifndef U3_SOLETAB_H */
