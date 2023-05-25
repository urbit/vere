/// @file

#include "serial.h"

#include <errno.h>
#include <math.h>
#include <fcntl.h>

#include "allocate.h"
#include "hashtable.h"
#include "jets/k.h"
#include "jets/q.h"
#include "retrieve.h"
#include "serial.h"
#include "ur.h"
#include "vortex.h"
#include "xtract.h"

const c3_y u3s_dit_y[64] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '-', '~'
};

/* _cs_jam_buf: struct for tracking the fibonacci-allocated jam of a noun
*/
struct _cs_jam_fib {
  u3i_slab*     sab_u;
  u3p(u3h_root) har_p;
  c3_w          a_w;
  c3_w          b_w;
  c3_w          bit_w;
};

/* _cs_jam_fib_grow(): reallocate buffer with fibonacci growth
*/
static void
_cs_jam_fib_grow(struct _cs_jam_fib* fib_u, c3_w mor_w)
{
  c3_w wan_w = fib_u->bit_w + mor_w;

  // check for c3_w overflow
  //
  if ( wan_w < mor_w ) {
    u3m_bail(c3__fail);
  }

  if ( wan_w > fib_u->a_w ) {
    c3_w   c_w = 0;

    //  fibonacci growth
    //
    while ( c_w < wan_w ) {
      c_w        = fib_u->a_w + fib_u->b_w;
      fib_u->b_w = fib_u->a_w;
      fib_u->a_w = c_w;
    }

    u3i_slab_grow(fib_u->sab_u, 0, c_w);
  }
}

/* _cs_jam_fib_chop(): chop [met_w] bits of [a] into [fib_u]
*/
static void
_cs_jam_fib_chop(struct _cs_jam_fib* fib_u, c3_w met_w, u3_noun a)
{
  c3_w bit_w = fib_u->bit_w;
  _cs_jam_fib_grow(fib_u, met_w);
  fib_u->bit_w += met_w;

  {
    c3_w* buf_w = fib_u->sab_u->buf_w;
    u3r_chop(0, 0, met_w, bit_w, buf_w, a);
  }
}

/* _cs_jam_fib_mat(): length-prefixed encode (mat) [a] into [fib_u]
*/
static void
_cs_jam_fib_mat(struct _cs_jam_fib* fib_u, u3_noun a)
{
  if ( 0 == a ) {
    _cs_jam_fib_chop(fib_u, 1, 1);
  }
  else {
    c3_w a_w = u3r_met(0, a);
    c3_w b_w = c3_bits_word(a_w);

    _cs_jam_fib_chop(fib_u, b_w+1, 1 << b_w);
    _cs_jam_fib_chop(fib_u, b_w-1, a_w & ((1 << (b_w-1)) - 1));
    _cs_jam_fib_chop(fib_u, a_w, a);
  }
}

/* _cs_jam_fib_atom_cb(): encode atom or backref
*/
static void
_cs_jam_fib_atom_cb(u3_atom a, void* ptr_v)
{
  struct _cs_jam_fib* fib_u = ptr_v;
  u3_weak b = u3h_git(fib_u->har_p, a);

  //  if [a] has no backref, encode atom and put cursor into [har_p]
  //
  if ( u3_none == b ) {
    u3h_put(fib_u->har_p, a, u3i_words(1, &(fib_u->bit_w)));
    _cs_jam_fib_chop(fib_u, 1, 0);
    _cs_jam_fib_mat(fib_u, a);
  }
  else {
    c3_w a_w = u3r_met(0, a);
    c3_w b_w = u3r_met(0, b);

    //  if [a] is smaller than the backref, encode atom
    //
    if ( a_w <= b_w ) {
      _cs_jam_fib_chop(fib_u, 1, 0);
      _cs_jam_fib_mat(fib_u, a);
    }
    //  otherwise, encode backref
    //
    else {
      _cs_jam_fib_chop(fib_u, 2, 3);
      _cs_jam_fib_mat(fib_u, b);
    }
  }
}

/* _cs_jam_fib_cell_cb(): encode cell or backref
*/
static c3_o
_cs_jam_fib_cell_cb(u3_noun a, void* ptr_v)
{
  struct _cs_jam_fib* fib_u = ptr_v;
  u3_weak b = u3h_git(fib_u->har_p, a);

  //  if [a] has no backref, encode cell and put cursor into [har_p]
  //
  if ( u3_none == b ) {
    u3h_put(fib_u->har_p, a, u3i_words(1, &(fib_u->bit_w)));
    _cs_jam_fib_chop(fib_u, 2, 1);
    return c3y;
  }
  //  otherwise, encode backref and shortcircuit traversal
  //
  else {
    _cs_jam_fib_chop(fib_u, 2, 3);
    _cs_jam_fib_mat(fib_u, b);
    return c3n;
  }
}

/* u3s_jam_fib(): jam without atom allocation.
**
**   returns atom-suitable words, and *bit_w will have
**   the length (in bits). return should be freed with u3a_wfree().
*/
c3_w
u3s_jam_fib(u3i_slab* sab_u, u3_noun a)
{
  struct _cs_jam_fib fib_u;
  fib_u.har_p = u3h_new();
  fib_u.sab_u = sab_u;

  //  fib(12) is small enough to be reasonably fast to allocate.
  //  fib(11) is needed to get fib(13).
  //
  //
  fib_u.a_w   = ur_fib12;
  fib_u.b_w   = ur_fib11;
  fib_u.bit_w = 0;
  u3i_slab_init(sab_u, 0, fib_u.a_w);

  u3a_walk_fore(a, &fib_u, _cs_jam_fib_atom_cb, _cs_jam_fib_cell_cb);

  u3h_free(fib_u.har_p);
  return fib_u.bit_w;
}

typedef struct _jam_xeno_s {
  u3p(u3h_root) har_p;
  ur_bsw_t      rit_u;
} _jam_xeno_t;

/* _cs_coin_chub(): shortcircuit u3i_chubs().
*/
static inline u3_atom
_cs_coin_chub(c3_d a_d)
{
  return ( 0x7fffffffULL >= a_d ) ? a_d : u3i_chubs(1, &a_d);
}

/* _cs_jam_xeno_atom(): encode in/direct atom in bitstream.
*/
static inline void
_cs_jam_bsw_atom(ur_bsw_t* rit_u, c3_w met_w, u3_atom a)
{
  if ( c3y == u3a_is_cat(a) ) {
    //  XX need a ur_bsw_atom32()
    //
    ur_bsw_atom64(rit_u, (c3_y)met_w, (c3_d)a);
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    //  XX assumes little-endian
    //  XX need a ur_bsw_atom_words()
    //
    c3_y*     byt_y = (c3_y*)vat_u->buf_w;
    ur_bsw_atom_bytes(rit_u, (c3_d)met_w, byt_y);
  }
}

/* _cs_jam_bsw_back(): encode in/direct backref in bitstream.
*/
static inline void
_cs_jam_bsw_back(ur_bsw_t* rit_u, c3_w met_w, u3_atom a)
{
  c3_d bak_d = ( c3y == u3a_is_cat(a) )
             ? (c3_d)a
             : u3r_chub(0, a);

  //  XX need a ur_bsw_back32()
  //
  ur_bsw_back64(rit_u, (c3_y)met_w, bak_d);
}

/* _cs_jam_xeno_atom(): encode atom or backref in bitstream.
*/
static void
_cs_jam_xeno_atom(u3_atom a, void* ptr_v)
{
  _jam_xeno_t* jam_u = ptr_v;
  ur_bsw_t*    rit_u = &(jam_u->rit_u);
  u3_weak        bak = u3h_git(jam_u->har_p, a);
  c3_w         met_w = u3r_met(0, a);

  if ( u3_none == bak ) {
    u3h_put(jam_u->har_p, a, _cs_coin_chub(rit_u->bits));
    _cs_jam_bsw_atom(rit_u, met_w, a);
  }
  else {
    c3_w bak_w = u3r_met(0, bak);

    if ( met_w <= bak_w ) {
      _cs_jam_bsw_atom(rit_u, met_w, a);
    }
    else {
      _cs_jam_bsw_back(rit_u, bak_w, bak);
    }
  }
}

/* _cs_jam_xeno_cell(): encode cell or backref in bitstream.
*/
static c3_o
_cs_jam_xeno_cell(u3_noun a, void* ptr_v)
{
  _jam_xeno_t* jam_u = ptr_v;
  ur_bsw_t*    rit_u = &(jam_u->rit_u);
  u3_weak        bak = u3h_git(jam_u->har_p, a);

  if ( u3_none == bak ) {
    u3h_put(jam_u->har_p, a, _cs_coin_chub(rit_u->bits));
    ur_bsw_cell(rit_u);
    return c3y;
  }
  else {
    _cs_jam_bsw_back(rit_u, u3r_met(0, bak), bak);
    return c3n;
  }
}

/* u3s_jam_xeno(): jam with off-loom buffer (re-)allocation.
*/
c3_d
u3s_jam_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y)
{
  _jam_xeno_t jam_u = {0};
  ur_bsw_init(&jam_u.rit_u, ur_fib11, ur_fib12);
  jam_u.har_p = u3h_new();

  u3a_walk_fore(a, &jam_u, _cs_jam_xeno_atom, _cs_jam_xeno_cell);

  u3h_free(jam_u.har_p);
  return ur_bsw_done(&jam_u.rit_u, len_d, byt_y);
}

/* _cs_cue: stack frame for tracking intermediate cell results
*/
typedef struct _cs_cue {
  u3_weak hed;  //  head of a cell or u3_none
  u3_atom wid;  //  bitwidth of [hed] or 0
  u3_atom cur;  //  bit-cursor position
} _cs_cue;

/* _cs_rub: rub, TRANSFER [cur], RETAIN [a]
*/
static inline u3_noun
_cs_rub(u3_atom cur, u3_atom a)
{
  u3_noun pro = u3qe_rub(cur, a);
  u3z(cur);
  return pro;
}

/* _cs_cue_next(): advance into [a], reading next value
**                 TRANSFER [cur], RETAIN [a]
*/
static inline u3_noun
_cs_cue_next(u3a_pile*     pil_u,
             u3p(u3h_root) har_p,
             u3_atom         cur,
             u3_atom           a,
             u3_atom*        wid)
{
  while ( 1 ) {
    //  read tag bit at cur
    //
    c3_y tag_y = u3qc_cut(0, cur, 1, a);

    //  low bit unset, (1 + cur) points to an atom
    //
    //    produce atom and the width we read
    //
    if ( 0 == tag_y ) {
      u3_noun bur = _cs_rub(u3i_vint(cur), a);
      u3_noun pro = u3k(u3t(bur));

      u3h_put(har_p, cur, u3k(pro));
      *wid = u3qa_inc(u3h(bur));

      u3z(bur);
      return pro;
    }
    else {
      //  read tag bit at (1 + cur)
      //
      {
        u3_noun x = u3qa_inc(cur);
        tag_y = u3qc_cut(0, x, 1, a);
        u3z(x);
      }

      //  next bit set, (2 + cur) points to a backref
      //
      //    produce referenced value and the width we read
      //
      if ( 1 == tag_y ) {
        u3_noun bur = _cs_rub(u3ka_add(2, cur), a);
        u3_noun pro = u3x_good(u3h_get(har_p, u3t(bur)));

        *wid = u3qa_add(2, u3h(bur));

        u3z(bur);
        return pro;
      }
      //  next bit unset, (2 + cur) points to the head of a cell
      //
      //    push a head-frame onto the road stack and read the head
      //
      else {
        _cs_cue* fam_u = u3a_push(pil_u);

        //  NB: fam_u->wid unused in head-frame
        //
        fam_u->hed = u3_none;
        fam_u->cur = cur;

        cur = u3qa_add(2, cur);
        continue;
      }
    }
  }
}

u3_noun
u3s_cue(u3_atom a)
{
  //  pro:   cue'd noun product
  //  wid:   bitwidth read to produce [pro]
  //  fam_u: stack frame
  //  har_p: backreference table
  //  pil_u: stack control structure
  //
  u3_noun         pro;
  u3_atom         wid;
  _cs_cue*      fam_u;
  u3p(u3h_root) har_p = u3h_new();
  u3a_pile      pil_u;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence cueing at bit-position 0
  //
  pro = _cs_cue_next(&pil_u, har_p, 0, a, &wid);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash [pro] and [wid]; continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        //  NB: fam_u->wid unused in head-frame
        //
        fam_u->hed = pro;
        fam_u->wid = wid;

        //  continue reading at the bit-position after [pro]
        {
          u3_noun cur = u3ka_add(2, u3qa_add(wid, fam_u->cur));
          pro = _cs_cue_next(&pil_u, har_p, cur, a, &wid);
        }

        fam_u = u3a_peek(&pil_u);
      }
      //  tail-frame: cons cell, recalculate [wid], and pop the stack
      //
      else {
        pro   = u3nc(fam_u->hed, pro);
        u3h_put(har_p, fam_u->cur, u3k(pro));
        u3z(fam_u->cur);
        wid   = u3ka_add(2, u3ka_add(wid, fam_u->wid));
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  u3z(wid);
  u3h_free(har_p);

  return pro;
}

/*
**  stack frame for recording head vs tail iteration
**
**    $?  [u3_none bits=@]
**    [hed=* bits=@]
*/
typedef struct _cue_frame_s {
  u3_weak ref;
  c3_d  bit_d;
} _cue_frame_t;

/* _cs_cue_xeno_next(): read next value from bitstream, dictionary off-loom.
*/
static inline ur_cue_res_e
_cs_cue_xeno_next(u3a_pile*    pil_u,
                  ur_bsr_t*    red_u,
                  ur_dict32_t* dic_u,
                  u3_noun*       out)
{
  ur_root_t* rot_u = 0;

  while ( 1 ) {
    c3_d  len_d, bit_d = red_u->bits;
    ur_cue_tag_e tag_e;
    ur_cue_res_e res_e;

    if ( ur_cue_good != (res_e = ur_bsr_tag(red_u, &tag_e)) ) {
      return res_e;
    }

    switch ( tag_e ) {
      default: u3_assert(0);

      case ur_jam_cell: {
        _cue_frame_t* fam_u = u3a_push(pil_u);

        fam_u->ref   = u3_none;
        fam_u->bit_d = bit_d;
        continue;
      }

      case ur_jam_back: {
        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }
        else if ( 62 < len_d ) {
          return ur_cue_meme;
        }
        else {
          c3_d bak_d = ur_bsr64_any(red_u, len_d);
          c3_w bak_w;

          if ( !ur_dict32_get(rot_u, dic_u, bak_d, &bak_w) ) {
            return ur_cue_back;
          }

          *out = u3k((u3_noun)bak_w);
          return ur_cue_good;
        }
      }

      case ur_jam_atom: {
        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }

        if ( 31 >= len_d ) {
          *out = (u3_noun)ur_bsr32_any(red_u, len_d);
        }
        else {
          c3_d     byt_d = (len_d + 0x7) >> 3;
          u3i_slab sab_u;

          if ( 0xffffffffULL < byt_d) {
            return ur_cue_meme;
          }
          else {
            u3i_slab_init(&sab_u, 3, byt_d);
            ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
            *out = u3i_slab_mint_bytes(&sab_u);
          }
        }

        ur_dict32_put(rot_u, dic_u, bit_d, *out);
        return ur_cue_good;
      }
    }
  }
}

struct _u3_cue_xeno {
  ur_dict32_t dic_u;
};

/* _cs_cue_xeno(): cue on-loom, with off-loom dictionary in handle.
*/
static u3_weak
_cs_cue_xeno(u3_cue_xeno* sil_u,
             c3_d         len_d,
             const c3_y*  byt_y)
{
  ur_bsr_t      red_u = {0};
  ur_dict32_t*  dic_u = &sil_u->dic_u;
  u3a_pile      pil_u;
  _cue_frame_t* fam_u;
  ur_cue_res_e  res_e;
  u3_noun         ref;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  init bitstream-reader
  //
  if ( ur_cue_good != (res_e = ur_bsr_init(&red_u, len_d, byt_y)) ) {
    return c3n;
  }
  //  bit-cursor (and backreferences) must fit in 62-bit direct atoms
  //
  else if ( 0x7ffffffffffffffULL < len_d ) {
    return c3n;
  }

  //  advance into stream
  //
  res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u, &ref);

  //  process cell results
  //
  if (  (c3n == u3a_pile_done(&pil_u))
     && (ur_cue_good == res_e) )
  {
    fam_u = u3a_peek(&pil_u);

    do {
      //  f is a head-frame; stash result and read the tail from the stream
      //
      if ( u3_none == fam_u->ref ) {
        fam_u->ref = ref;
        res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u, &ref);
        fam_u = u3a_peek(&pil_u);
      }
      //  f is a tail-frame; pop the stack and continue
      //
      else {
        ur_root_t* rot_u = 0;

        ref   = u3nc(fam_u->ref, ref);
        ur_dict32_put(rot_u, dic_u, fam_u->bit_d, ref);
        fam_u = u3a_pop(&pil_u);
      }
    }
    while (  (c3n == u3a_pile_done(&pil_u))
          && (ur_cue_good == res_e) );
  }

  if ( ur_cue_good == res_e ) {
    return ref;
  }
  //  on failure, unwind the stack and dispose of intermediate nouns
  //
  else if ( c3n == u3a_pile_done(&pil_u) ) {
    do {
      if ( u3_none != fam_u->ref ) {
        u3z(fam_u->ref);
      }
      fam_u = u3a_pop(&pil_u);
    }
    while ( c3n == u3a_pile_done(&pil_u) );
  }

  return u3_none;
}

/* u3s_cue_xeno_init_with(): initialize a cue_xeno handle as specified.
*/
u3_cue_xeno*
u3s_cue_xeno_init_with(c3_d pre_d, c3_d siz_d)
{
  u3_cue_xeno* sil_u;

  u3_assert( &(u3H->rod_u) == u3R );

  sil_u = c3_calloc(sizeof(*sil_u));
  ur_dict32_grow((ur_root_t*)0, &sil_u->dic_u, pre_d, siz_d);

  return sil_u;
}

/* u3s_cue_xeno_init(): initialize a cue_xeno handle.
*/
u3_cue_xeno*
u3s_cue_xeno_init(void)
{
  return u3s_cue_xeno_init_with(ur_fib10, ur_fib11);
}

/* u3s_cue_xeno_init(): cue on-loom, with off-loom dictionary in handle.
*/
u3_weak
u3s_cue_xeno_with(u3_cue_xeno* sil_u,
                  c3_d         len_d,
                  const c3_y*  byt_y)
{
  u3_weak som;

  u3_assert( &(u3H->rod_u) == u3R );

  som = _cs_cue_xeno(sil_u, len_d, byt_y);
  ur_dict32_wipe(&sil_u->dic_u);
  return som;
}

/* u3s_cue_xeno_init(): dispose cue_xeno handle.
*/
void
u3s_cue_xeno_done(u3_cue_xeno* sil_u)
{
  ur_dict_free((ur_dict_t*)&sil_u->dic_u);
  c3_free(sil_u);
}

/* u3s_cue_xeno(): cue on-loom, with off-loom dictionary.
*/
u3_weak
u3s_cue_xeno(c3_d        len_d,
             const c3_y* byt_y)
{
  u3_cue_xeno* sil_u;
  u3_weak        som;

  u3_assert( &(u3H->rod_u) == u3R );

  sil_u = u3s_cue_xeno_init();
  som   = _cs_cue_xeno(sil_u, len_d, byt_y);
  u3s_cue_xeno_done(sil_u);
  return som;
}

/* _cs_cue_need(): bail on ur_cue_* read failures.
*/
static inline void
_cs_cue_need(ur_cue_res_e res_e)
{
  if ( ur_cue_good == res_e ) {
    return;
  }
  else {
    c3_m mot_m = (ur_cue_meme == res_e) ? c3__meme : c3__exit;
    u3m_bail(mot_m);
  }
}

/* _cs_cue_get(): u3h_get wrapper handling allocation and refcounts.
*/
static inline u3_weak
_cs_cue_get(u3p(u3h_root) har_p, c3_d key_d)
{
  u3_atom key = _cs_coin_chub(key_d);
  u3_weak pro = u3h_get(har_p, key);
  u3z(key);
  return pro;
}

/* _cs_cue_put(): u3h_put wrapper handling allocation and refcounts.
*/
static inline u3_noun
_cs_cue_put(u3p(u3h_root) har_p, c3_d key_d, u3_noun val)
{
  u3_atom key = _cs_coin_chub(key_d);
  u3h_put(har_p, key, u3k(val));
  u3z(key);
  return val;
}

/* _cs_cue_bytes_next(): read next value from bitstream.
*/
static inline u3_noun
_cs_cue_bytes_next(u3a_pile*     pil_u,
                   u3p(u3h_root) har_p,
                   ur_bsr_t*     red_u)
{
  while ( 1 ) {
    c3_d  len_d, bit_d = red_u->bits;
    ur_cue_tag_e tag_e;

    _cs_cue_need(ur_bsr_tag(red_u, &tag_e));

    switch ( tag_e ) {
      default: u3_assert(0);

      case ur_jam_cell: {
        _cue_frame_t* fam_u = u3a_push(pil_u);

        fam_u->ref   = u3_none;
        fam_u->bit_d = bit_d;
        continue;
      }

      case ur_jam_back: {
        _cs_cue_need(ur_bsr_rub_len(red_u, &len_d));

        if ( 62 < len_d ) {
          return u3m_bail(c3__meme);
        }
        else {
          c3_d  bak_d = ur_bsr64_any(red_u, len_d);
          u3_weak bak = _cs_cue_get(har_p, bak_d);
          return u3x_good(bak);
        }
      }

      case ur_jam_atom: {
        u3_atom vat;

        _cs_cue_need(ur_bsr_rub_len(red_u, &len_d));

        if ( 31 >= len_d ) {
          vat = (u3_noun)ur_bsr32_any(red_u, len_d);
        }
        else {
          u3i_slab sab_u;
          u3i_slab_init(&sab_u, 0, len_d);

          ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
          vat = u3i_slab_mint_bytes(&sab_u);
        }

        return _cs_cue_put(har_p, bit_d, vat);
      }
    }
  }
}

/* u3s_cue_bytes(): cue bytes onto the loom.
*/
u3_noun
u3s_cue_bytes(c3_d len_d, const c3_y* byt_y)
{
  ur_bsr_t      red_u = {0};
  u3a_pile      pil_u;
  _cue_frame_t* fam_u;
  u3p(u3h_root) har_p;
  u3_noun         ref;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  initialize a hash table for dereferencing backrefs
  //
  har_p = u3h_new();

  //  init bitstream-reader
  //
  _cs_cue_need(ur_bsr_init(&red_u, len_d, byt_y));

  //  bit-cursor (and backreferences) must fit in 62-bit direct atoms
  //
  if ( 0x7ffffffffffffffULL < len_d ) {
    return u3m_bail(c3__meme);
  }

  //  advance into stream
  //
  ref = _cs_cue_bytes_next(&pil_u, har_p, &red_u);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  f is a head-frame; stash result and read the tail from the stream
      //
      if ( u3_none == fam_u->ref ) {
        fam_u->ref = ref;
        ref        = _cs_cue_bytes_next(&pil_u, har_p, &red_u);
        fam_u      = u3a_peek(&pil_u);
      }
      //  f is a tail-frame; pop the stack and continue
      //
      else {
        ref   = u3nc(fam_u->ref, ref);
        _cs_cue_put(har_p, fam_u->bit_d, ref);
        fam_u = u3a_pop(&pil_u);
      }
    }
    while ( c3n == u3a_pile_done(&pil_u) );
  }

  u3h_free(har_p);

  return ref;
}

/* u3s_cue_atom(): cue atom.
*/
u3_noun
u3s_cue_atom(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_cue_bytes((c3_d)len_w, byt_y);
}

/* Compute the length of the address
 */
size_t _cs_etch_p_size(mpz_t a_mp) {

  size_t syb_i = mpz_sizeinbase(a_mp, 256);

  // 3 characters per syllabe, - every 2 syllabes, and -- every 8 syllabes,
  // and ~
  size_t len_i = syb_i * 3 + (syb_i / 2) + (syb_i / 8) + 1;

  // Discount hep at the boundary of 2 syllabes
  if ( 0 == (syb_i % 2) ) {
    len_i -= 1;
  }

  // Discount hep at the boundary of 8 syllabes
  if ( 0 == (syb_i % 8) ) {
    len_i -= 1;
  }

  return len_i;
}

/* _cs_etch_p_bytes: atom to @p impl.
 */
c3_y*
_cs_etch_p_bytes(mpz_t sxz_mp, c3_w len_w, c3_y* hun_y)
{
  c3_y* byt_y = hun_y + len_w - 1;

  // Comets and below
  //
  c3_d sxz;
  c3_s huk, hi, lo;

  // Process in chunks of 64 bits
  //
  while ( mpz_size(sxz_mp) ) {

    sxz = (c3_d) mpz_get_ui(sxz_mp);
    mpz_tdiv_q_2exp(sxz_mp, sxz_mp, 64);

    while ( sxz ) {

      huk = sxz & 0xffff;

      hi = huk >> 8;
      lo = huk & 0xff;

      u3_po_to_suffix(lo, byt_y - 2, byt_y - 1, byt_y);
      u3_po_to_prefix(hi, byt_y - 5, byt_y - 4,  byt_y - 3);

      sxz >>= 16;
      byt_y -= 6;
      len_w -= 6;

      // Print - every two syllabes
      if ( sxz ) {
        *byt_y = '-';
        byt_y--;
        len_w--;
      }
    }

    // Print -- every four syllabes
    if ( mpz_size(sxz_mp) ) {
      *byt_y = '-';
      *(byt_y - 1) = '-';

      byt_y -= 2;
      len_w -= 2;
    }
  }

  *byt_y = '~';

  return byt_y;
}


/* u3s_etch_p_smol(): c3_d to @p
**
**   =(28 (met 3 (scot %p (dec (bex 64)))))
*/
c3_y*
u3s_etch_p_smol(c3_d sxz, c3_y hun_y[SMOL_P])
{
  c3_y* byt_y = hun_y + SMOL_P - 1;

  // Galaxy
  //
  if ( sxz <= 0xff) {

    u3_po_to_suffix(sxz & 0xff, byt_y - 2, byt_y - 1, byt_y);
    byt_y -= 3;

    *byt_y = '~';

    return byt_y;
  }

  // Stars, planets and moons
  //
  c3_s huk, hi, lo;

  while ( sxz ) {

    huk = sxz & 0xffff;

    hi = huk >> 8;
    lo = huk & 0xff;

    u3_po_to_suffix(lo, byt_y - 2, byt_y - 1, byt_y);
    u3_po_to_prefix(hi, byt_y - 5, byt_y - 4,  byt_y - 3);

    sxz >>= 16;
    byt_y -= 6;

    // Print a separator every two syllabes
    if ( sxz ) {
      *byt_y = '-';
      byt_y--;
    }
  }

  *byt_y = '~';

  return byt_y;
}

/* u3s_etch_p_c(): atom to @p, as a malloc'd c string.
 */
size_t
u3s_etch_p_c(u3_atom a, c3_c** out_c)
{

  c3_d a_d;
  size_t len_i;
  c3_y* buf_y;

  u3_atom sxz = a;
  c3_o fen_o = c3n;

  // We only need to unscramble planets and below
  //
  if ( c3n == u3a_is_cat(a) ||
      (c3y == u3a_is_cat(a) && a >= 0x10000) ) {

    sxz = u3qe_fein_ob(a);
    fen_o = c3y;
  }

  if ( c3y == u3r_safe_chub(sxz, &a_d) ) {
    c3_y hun_y[SMOL_P];

    buf_y = u3s_etch_p_smol(a_d, hun_y);
    len_i = SMOL_P - ((c3_p)buf_y - (c3_p)hun_y);

    *out_c = c3_malloc(len_i + 1);
    (*out_c)[len_i] = 0;
    memcpy(*out_c, buf_y, len_i);

    if ( _(fen_o) ) {
      u3z(sxz);
    }

    return len_i;
  }

  mpz_t     sxz_mp;
  u3r_mp(sxz_mp, sxz);

  len_i = _cs_etch_p_size(sxz_mp);
  buf_y = malloc(len_i+1);
  buf_y[len_i] = 0;

  _cs_etch_p_bytes(sxz_mp, len_i, buf_y);

  *out_c = (c3_c*)buf_y;

  if ( _(fen_o) ) {
    u3z(sxz);
  }
  mpz_clear(sxz_mp);
  return len_i;
}

/* u3s_etch_p(): atom to @p.
 */
u3_atom
u3s_etch_p(u3_atom a)
{
  c3_d a_d;

  u3_atom sxz = a;
  c3_o fen_o = c3n;

  // We only need to unscramble planets and below
  //
  if ( c3n == u3a_is_cat(a) ||
      (c3y == u3a_is_cat(a) && a >= 0x10000) ) {

    sxz = u3qe_fein_ob(a);
    fen_o = c3y;
  }

  if ( c3y == u3r_safe_chub(sxz, &a_d) ) {
    c3_y  hun_y[SMOL_P];
    c3_y* buf_y = u3s_etch_p_smol(a_d, hun_y);
    c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;

    if ( _(fen_o) ) {
      u3z(sxz);
    }
    return u3i_bytes(SMOL_P - dif_w, buf_y);
  }

  u3i_slab sab_u;
  size_t   len_i;
  mpz_t     sxz_mp;
  u3r_mp(sxz_mp, sxz);

  len_i = _cs_etch_p_size(sxz_mp);
  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_p_bytes(sxz_mp, len_i, sab_u.buf_y);

  if ( _(fen_o) ) {
    u3z(sxz);
  }
  mpz_clear(sxz_mp);
  return u3i_slab_mint_bytes(&sab_u);
}

/* +yo time constants
 */
#define CET_YO 36524
#define DAY_YO 86400
#define ERA_YO 146097
#define HOR_YO 3600
#define MIT_YO 60
#define JES_YO 292277024400

static c3_s _cs_moh_yo[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static c3_s _cs_moy_yo[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* +tarp - parsed time
 */
struct _tarp {

  // days
  union {
    mpz_t day_mp;  // big
    c3_d  day_d;   // smol
  };
  c3_o dag_o;  // is day big?

  c3_w  hor_w;  // hours
  c3_w  mit_w;  // minutes
  c3_w  sec_w;  // seconds

  c3_d  fan_d;  // fractional seconds
  size_t cuk_i; // number 4-bit chunks in fan_d

};

/* [year month day]
 */
struct _cald {

  // year
  union {
    mpz_t yer_mp;
    c3_d  yer_d;
  };

  // year AD?
  c3_y yad_o;

  c3_s mot_s; // month
  c3_s day_s; // day
};

static size_t _cs_etch_da_size(struct _tarp* rip, struct _cald* ger)
{
  size_t len_i = 0;

  // Space for fractional part
  //
  if ( rip->fan_d > 0 ) {

    len_i += 4*rip->cuk_i + rip->cuk_i; // cuk_i 2-byte chunks + cuk_i .

    len_i += 1; // .

    len_i += 8+2; // hh.mm.ss + ..
  }

  // No fractional part
  //
  else {

    // Time hh.mm.ss
    if ( !( rip->hor_w == 0 && rip->mit_w == 0 && rip->sec_w == 0) ) {
      len_i += 8 + 2; // + ..
    }
  }

  // year.month.day
  //
  if ( ger->day_s < 10 ) {
    len_i += 1;
  }
  else {
    len_i += 2;
  }

  len_i += 1; // .

  if( ger->mot_s < 10 ) {
    len_i += 1;
  }
  else {
    len_i += 2;
  }

  len_i += 1; // .

  if ( _(rip->dag_o) ) {
    len_i += mpz_sizeinbase(ger->yer_mp, 10);
  }
  else {
    len_i += ceil(log10(ger->yer_d)) + 1;
  }

  if ( !_(ger->yad_o) ) {
    len_i += 1; // -
  }

  len_i += 1; // ~

  return len_i;
}

/* +yell: atom to +tarp
 */
static struct _tarp _cs_yell(u3_atom a) {

  u3_atom sec_a;
  mpz_t sec_mp;

  struct _tarp rip;

  rip.fan_d = u3r_chub(0,a);
  rip.cuk_i = 4;

  // Shift right until first non-zero chunk
  //
  if (rip.fan_d > 0) {
    while ( (rip.fan_d & 0xffff) == 0) {
      rip.fan_d >>= 16;
      rip.cuk_i--;
    }
  }
  else {
    rip.cuk_i = 0;
  }

  c3_d sec_d;
  sec_a = u3qc_rsh(6, 1, a);

  if ( c3y == u3r_safe_chub(sec_a, &sec_d) ) {
    rip.day_d = sec_d / DAY_YO;
    rip.sec_w = sec_d % DAY_YO;

    rip.dag_o = c3n;
  }

  else {
    u3r_mp(sec_mp, sec_a);
    mpz_init2(rip.day_mp, 64+32);

    mpz_tdiv_qr_ui(rip.day_mp, sec_mp, sec_mp, DAY_YO);
    rip.sec_w = mpz_get_ui(sec_mp);

    if ( mpz_fits_ulong_p(rip.day_mp) ) {
      // XX make sure this C is solid
      rip.day_d = mpz_get_ui(rip.day_mp);

      mpz_clear(rip.day_mp);

      rip.dag_o = c3n;
    }
    else {
      rip.dag_o = c3y;
    }

    mpz_clear(sec_mp);
  }

  rip.hor_w = rip.sec_w / HOR_YO;
  rip.sec_w %= HOR_YO;

  rip.mit_w = rip.sec_w / MIT_YO;
  rip.sec_w %= MIT_YO;


  u3z(sec_a);
  return rip;
}

/* +yall: small day / day of year
 */
static struct _cald _cs_yall_smol(c3_d day_d) {

  c3_d  era_d;
  c3_d  cet_d;
  c3_y  lep_o;

  struct _cald ger;

  era_d = 0;
  ger.yer_d = 0;

  era_d = day_d / ERA_YO;
  day_d %= ERA_YO;

  // We are within the first century,
  // and the first year is a leap year, despite
  // it being a centurial year -- it is divisible by 400
  //
  if ( day_d <= CET_YO ) {
    lep_o = c3y;
    cet_d = 0;
  }

  // We are past the first century
  //
  else {
    lep_o = c3n;
    cet_d = 1;
    day_d -= (CET_YO + 1);

    cet_d += day_d / CET_YO;
    day_d = day_d % CET_YO;

    // yer <- yer + cet*100
    ger.yer_d += cet_d*100;

  }

  // yer <- yer + era*400
  ger.yer_d += era_d*400;

  c3_d dis_d;
  c3_d ner_d = 0;

  if ( _(lep_o) ) {
    dis_d = 366;
  }
  else {
    dis_d = 365;
  }

  // Exceeded a year
  //
  while ( day_d >= dis_d ) {
    ner_d += 1;
    day_d -= dis_d;

    // leap year
    //
    if ( !(ner_d % 4) ) {
      lep_o = c3y;
      dis_d = 366;
    }
    else {
      lep_o = c3n;
      dis_d = 365;
    }
  }

  ger.yer_d += ner_d;

  c3_s* cah;
  ger.mot_s = 0;

  if ( _(lep_o) ) {
    cah = _cs_moy_yo;
  }
  else {
    cah = _cs_moh_yo;
  }

  // At this point day_d < 366
  ger.day_s = day_d;

  // Count towards the month
  //
  while ( ger.day_s >= cah[ger.mot_s] ) {
    ger.day_s -= cah[ger.mot_s];
    ger.mot_s++;
  }

  ger.day_s++;
  ger.mot_s++;

  // Year before Christ
  //
  if ( ger.yer_d <= JES_YO ) {
    ger.yer_d = 1 + JES_YO - ger.yer_d; // 0 AD is 1 BC
    ger.yad_o = c3n;
  }

  // Year after Christ
  //
  else {
    ger.yer_d -= JES_YO;
    ger.yad_o = c3y;
  }

  return ger;
}
/* +yall: day / day of year
 */
static struct _cald _cs_yall(mpz_t day_mp) {

  mpz_t era_mp;
  c3_d  cet_d;
  c3_d  day_d;
  c3_y  lep_o;

  struct _cald ger;

  mpz_init(era_mp);
  mpz_init(ger.yer_mp);

  mpz_tdiv_qr_ui(era_mp, day_mp, day_mp, ERA_YO);

  day_d = mpz_get_ui(day_mp);

  // We are within the first century,
  // and the first year is a leap year, despite
  // it being a centurial year -- it is divisible by 400
  //
  if ( day_d <= CET_YO ) {
    lep_o = c3y;
    cet_d = 0;
  }

  // We are past the first century
  //
  else {
    lep_o = c3n;
    cet_d = 1;
    day_d -= (CET_YO + 1);

    cet_d += day_d / CET_YO;
    day_d = day_d % CET_YO;

    // yer <- yer + cet*100
    mpz_add_ui(ger.yer_mp, ger.yer_mp, cet_d*100);

  }

  // yer <- yer + era*400
  mpz_addmul_ui(ger.yer_mp, era_mp, 400);

  c3_d dis_d;
  c3_d ner_d = 0;

  if ( _(lep_o) ) {
    dis_d = 366;
  }
  else {
    dis_d = 365;
  }

  // Exceeded a year
  //
  while ( day_d >= dis_d ) {
    ner_d += 1;
    day_d -= dis_d;

    // leap year
    //
    if ( !(ner_d % 4) ) {
      lep_o = c3y;
      dis_d = 366;
    }
    else {
      lep_o = c3n;
      dis_d = 365;
    }
  }

  mpz_add_ui(ger.yer_mp, ger.yer_mp, ner_d);

  c3_s* cah;
  ger.mot_s = 0;

  if ( _(lep_o) ) {
    cah = _cs_moy_yo;
  }
  else {
    cah = _cs_moh_yo;
  }

  // At this point day_d < 366
  ger.day_s = day_d;

  // Count towards the month
  //
  while ( ger.day_s >= cah[ger.mot_s] ) {
    ger.day_s -= cah[ger.mot_s];
    ger.mot_s++;
  }

  ger.day_s++;
  ger.mot_s++;

  // Year before Christ
  //
  if ( mpz_cmp_ui(ger.yer_mp, JES_YO) <= 0 ) {

    mpz_ui_sub(ger.yer_mp, 1 + JES_YO, ger.yer_mp);
    ger.yad_o = c3n;
  }

  // Year after Christ
  //
  else {
    mpz_sub_ui(ger.yer_mp, ger.yer_mp, JES_YO);
    ger.yad_o = c3y;
  }

  mpz_clear(era_mp);
  return ger;
}
/* _cs_etch_da_bytes(): atom to @da impl.
 */
size_t  _cs_etch_da_bytes(struct _tarp* rip, struct _cald* ger, size_t len_i, c3_y* hun_y)
{

  c3_y* buf_y = hun_y + (len_i - 1);

  c3_s paf_s = 0x0;

  // Print out up to four 16-bit chunks
  //
  if ( rip->fan_d > 0) {

    while ( rip->cuk_i > 0) {

      paf_s = rip->fan_d & 0xffff;
      rip->fan_d >>= 16;

      // Print the 16-bit chunk in hexadecimal
      *buf_y-- = u3s_dit_y[ (paf_s >> 0) & 0xf ];
      *buf_y-- = u3s_dit_y[ (paf_s >> 4) & 0xf ];
      *buf_y-- = u3s_dit_y[ (paf_s >> 8) & 0xf ];
      *buf_y-- = u3s_dit_y[ (paf_s >> 12) & 0xf ];
      *buf_y-- = '.';

      rip->cuk_i--;
    }

    *buf_y-- = '.';
  }

  // Print the time if the time is non-zero, or
  // if we have printed fractional seconds
  //
  if ( buf_y < (hun_y + len_i - 1) || ! ( rip->fan_d == 0 && rip->hor_w == 0 && rip->mit_w == 0 && rip->sec_w == 0 ) ) {

      *buf_y-- = '0' + ( rip->sec_w % 10 );
      *buf_y-- = '0' + ( rip->sec_w / 10 );
      *buf_y-- = '.';

      *buf_y-- = '0' + ( rip->mit_w % 10 );
      *buf_y-- = '0' + ( rip->mit_w / 10 );
      *buf_y-- = '.';

      *buf_y-- = '0' + ( rip->hor_w % 10 );
      *buf_y-- = '0' + ( rip->hor_w / 10 );
      *buf_y-- = '.';
      *buf_y-- = '.';
  }

  // print out the day of the year
  //
  *buf_y-- = '0' + ( ger->day_s % 10);
  ger->day_s /= 10;
  if ( ger->day_s > 0 ) {
    *buf_y-- = '0' + ger->day_s;
  }
  *buf_y-- = '.';

  *buf_y-- = '0' + ( ger->mot_s % 10);
  ger->mot_s /= 10;
  if ( ger->mot_s > 0 ) {
    *buf_y-- = '0' + ger->mot_s;
  }
  *buf_y-- = '.';

  // Print out the year
  //

  // BC year
  if( ger->yad_o == c3n ) {
    *buf_y-- = '-';
  }


  // Smol year
  //
  if ( !_(rip->dag_o) ) {
    while ( ger->yer_d > 0 ) {
      *buf_y-- = '0' + ger->yer_d % 10;
      ger->yer_d /= 10;
    }
  }

  else {

    mpz_t r_mp;
    mpz_init(r_mp);
    c3_d dit_d;

    // XX speed up by reading whole words ?
    //
    while ( mpz_size(ger->yer_mp) > 0 ) {

      dit_d = mpz_tdiv_qr_ui(ger->yer_mp, r_mp, ger->yer_mp, 10);
      *buf_y-- = '0' + dit_d;
    }

    mpz_clear(r_mp);
  }

  *buf_y = '~';

  size_t dif_i = buf_y - hun_y;

  if ( dif_i > 0 ) {
    len_i -= dif_i;
    memmove(hun_y, buf_y, len_i);
    memset(hun_y + len_i, 0, dif_i);
  }

  return len_i;
}

/* _u3s_etch_da: atom to @da.
 */
u3_atom
u3s_etch_da(u3_atom a)
{

  struct _tarp rip;
  struct _cald ger;
  size_t len_i;

  rip = _cs_yell(a);

  if ( !_(rip.dag_o) ) {
    ger = _cs_yall_smol(rip.day_d);
  }
  else {
    ger = _cs_yall(rip.day_mp);
  }

  len_i = _cs_etch_da_size(&rip, &ger);

  u3i_slab sab_u;
  c3_y* buf_y;

  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_da_bytes(&rip, &ger, len_i, sab_u.buf_y);

  if ( _(rip.dag_o) ) {
    mpz_clear(rip.day_mp);
    mpz_clear(ger.yer_mp);
  }
  return u3i_slab_mint_bytes(&sab_u);
}

/* u3s_etch_da_c: atom to @da, as a malloc'd c string.
 */
size_t
u3s_etch_da_c(u3_atom a, c3_c** out_c)
{
  c3_y* buf_y;
  size_t len_i;
  struct _tarp rip;
  struct _cald ger;

  rip = _cs_yell(a);

  if ( !_(rip.dag_o) ) {
    ger = _cs_yall_smol(rip.day_d);
  }
  else {
    ger = _cs_yall(rip.day_mp);
  }

  len_i = _cs_etch_da_size(&rip, &ger);

  buf_y = c3_malloc(len_i + 1);
  buf_y[len_i] = 0;

  len_i = _cs_etch_da_bytes(&rip, &ger, len_i, buf_y);

  *out_c = (c3_c*)buf_y;

  if ( _(rip.dag_o) ) {
    mpz_clear(rip.day_mp);
    mpz_clear(ger.yer_mp);
  }
  return len_i;
}

/* _cs_etch_ud_size(): output length in @ud for given mpz_t.
*/
static inline size_t
_cs_etch_ud_size(mpz_t a_mp)
{
  size_t len_i = mpz_sizeinbase(a_mp, 10);
  return len_i + (len_i / 3); // separators
}

/* _cs_etch_ud_bytes(): atom to @ud impl.
*/
static size_t
_cs_etch_ud_bytes(mpz_t a_mp, size_t len_i, c3_y* hun_y)
{
  c3_y*   buf_y = hun_y + (len_i - 1);
  mpz_t   b_mp;
  c3_w     b_w;
  size_t dif_i;

  mpz_init2(b_mp, 10);

  if ( !mpz_size(a_mp) ) {
    *buf_y-- = '0';
  }
  else {
    while ( 1 ) {
      b_w = mpz_tdiv_qr_ui(a_mp, b_mp, a_mp, 1000);

      if ( !mpz_size(a_mp) ) {
        while ( b_w ) {
          *buf_y-- = '0' + (b_w % 10);
          b_w /= 10;
        }
        break;
      }

      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      *buf_y-- = '.';
    }
  }

  buf_y++;

  //  mpz_sizeinbase may overestimate by 1
  //
  {
    size_t dif_i = buf_y - hun_y;

    if ( dif_i ) {
      len_i -= dif_i;
      memmove(hun_y, buf_y, len_i);
      memset(hun_y + len_i, 0, dif_i);
    }
  }

  mpz_clear(b_mp);
  return len_i;
}

/* u3s_etch_ud_smol(): c3_d to @ud
**
**   =(26 (met 3 (scot %ud (dec (bex 64)))))
*/
c3_y*
u3s_etch_ud_smol(c3_d a_d, c3_y hun_y[SMOL_UD])
{
  c3_y*  buf_y = hun_y + SMOL_UD - 1;
  c3_w     b_w;

  if ( !a_d ) {
    *buf_y-- = '0';
  }

  else {
    while ( 1 ) {
      b_w  = a_d % 1000;
      a_d /= 1000;

      if ( !a_d ) {
        while ( b_w ) {
          *buf_y-- = '0' + (b_w % 10);
          b_w /= 10;
        }
        break;
      }

      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      *buf_y-- = '.';
    }
  }

  return buf_y + 1;
}

/* u3s_etch_ud(): atom to @ud.
*/
u3_atom
u3s_etch_ud(u3_atom a)
{
  c3_d a_d;

  if ( c3y == u3r_safe_chub(a, &a_d) ) {
    c3_y  hun_y[SMOL_UD];
    c3_y* buf_y = u3s_etch_ud_smol(a_d, hun_y);
    c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;
    return u3i_bytes(SMOL_UD - dif_w, buf_y);
  }

  u3i_slab sab_u;
  size_t   len_i;
  mpz_t     a_mp;
  u3r_mp(a_mp, a);

  len_i = _cs_etch_ud_size(a_mp);
  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_ud_bytes(a_mp, len_i, sab_u.buf_y);

  mpz_clear(a_mp);
  return u3i_slab_mint_bytes(&sab_u);
}

/* u3s_etch_ud_c(): atom to @ud, as a malloc'd c string.
*/
size_t
u3s_etch_ud_c(u3_atom a, c3_c** out_c)
{
  c3_d     a_d;
  size_t len_i;
  c3_y*  buf_y;

  if ( c3y == u3r_safe_chub(a, &a_d) ) {
    c3_y  hun_y[SMOL_UD];

    buf_y = u3s_etch_ud_smol(a_d, hun_y);
    len_i = SMOL_UD - ((c3_p)buf_y - (c3_p)hun_y);

    *out_c = c3_malloc(len_i + 1);
    (*out_c)[len_i] = 0;
    memcpy(*out_c, buf_y, len_i);

    return len_i;
  }

  mpz_t   a_mp;
  u3r_mp(a_mp, a);

  len_i = _cs_etch_ud_size(a_mp);
  buf_y = c3_malloc(len_i + 1);
  buf_y[len_i] = 0;

  len_i = _cs_etch_ud_bytes(a_mp, len_i, buf_y);

  *out_c = (c3_c*)buf_y;

  mpz_clear(a_mp);
  return len_i;
}

/* _cs_etch_ui_size(): output length in @ui for given mpz_t
 */
static inline size_t
_cs_etch_ui_size(mpz_t a_mp)
{
    size_t len_i = mpz_sizeinbase(a_mp, 10);
    return len_i + 2; // + 0i
}

/* _cs_etch_ui_bytes(): atom to @ui impl.
 */
static size_t
_cs_etch_ui_bytes(mpz_t a_mp, size_t len_i, c3_y* hun_y)
{
    c3_y* buf_y = hun_y + (len_i - 1);
    c3_w    b_w;
    size_t  dif_i;

    if ( !mpz_size(a_mp) ) {
        *buf_y-- = '0';
    }
    else {
        while ( mpz_size(a_mp) ) {

            // 9 digits fit into a word
            b_w = mpz_tdiv_q_ui(a_mp, a_mp, 1000000000);

            while ( b_w ) {
                *buf_y-- = '0' + (b_w % 10);
                b_w /= 10;
            }
        }
    }

    *buf_y-- = 'i';
    *buf_y = '0';

    // XX mpz_sizeinbase may overestimate by 1
    {
        size_t dif_i = buf_y - hun_y;

        if ( dif_i ) {
            len_i -= dif_i;
            memmove(hun_y, buf_y, len_i);
            memset(hun_y + len_i, 0, dif_i);
        }
    }

    return len_i;
}

/* u3s_etch_ui_smol(): c3_d to @ui
 **
 **  =(22 (met 3 (scot %ud (dec (bex 64)))))
 */
c3_y*
u3s_etch_ui_smol(c3_d a_d, c3_y hun_y[SMOL_UI])
{
    c3_y* buf_y = hun_y + SMOL_UI - 1;
    c3_w  b_w;

    if ( !a_d ) {
        *buf_y-- = '0';
    }
    else{
        while ( a_d > 0 ) {
            b_w = a_d % 10;
            a_d /= 10;

            *buf_y-- = '0' + b_w;
        }
    }

    *buf_y-- = 'i';
    *buf_y-- = '0';

    return buf_y + 1;
}

/* u3s_etch_ui(): atom to @ui.
 */
u3_atom
u3s_etch_ui(u3_atom a)
{
    c3_d a_d;

    if ( c3y == u3r_safe_chub(a, &a_d) ) {
        c3_y hun_y[SMOL_UI];
        c3_y* buf_y = u3s_etch_ui_smol(a_d, hun_y);
        c3_w dif_w = (c3_p)buf_y - (c3_p)hun_y;
        return u3i_bytes(SMOL_UI - dif_w, buf_y);
    }

    u3i_slab sab_u;
    size_t   len_i;
    mpz_t    a_mp;
    u3r_mp(a_mp, a);

    len_i = _cs_etch_ui_size(a_mp);
    u3i_slab_bare(&sab_u, 3, len_i);
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    _cs_etch_ui_bytes(a_mp, len_i, sab_u.buf_y);

    mpz_clear(a_mp);
    return u3i_slab_mint_bytes(&sab_u);
}

/* u3s_etch_ui_c(): atom to @ui, as a malloc'd c string.
 */
size_t
u3s_etch_ui_c(u3_atom a, c3_c** out_c)
{
    c3_d   a_d;
    size_t len_i;
    c3_y*  buf_y;

    if ( c3y == u3r_safe_chub(a, &a_d) ) {
        c3_y hun_y[SMOL_UI];
        buf_y = u3s_etch_ui_smol(a_d, hun_y);
        len_i = SMOL_UI - ((c3_p)buf_y - (c3_p)hun_y);
        *out_c = c3_malloc(len_i + 1);
        (*out_c)[len_i] = 0;
        memcpy(*out_c, buf_y, len_i);

        return len_i;
    }

    mpz_t a_mp;
    u3r_mp(a_mp, a);

    len_i = _cs_etch_ui_size(a_mp);
    buf_y = c3_malloc(len_i + 1);
    buf_y[len_i] = 0;

    len_i = _cs_etch_ui_bytes(a_mp, len_i, buf_y);

    *out_c = (c3_c*)buf_y;

    mpz_clear(a_mp);
    return len_i;
}

/* _cs_etch_ux_bytes(): atom to @ux impl.
*/
static void
_cs_etch_ux_bytes(u3_atom a, c3_w len_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_s inp_s;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    inp_s = u3r_short(i_w, a);

    *buf_y-- = u3s_dit_y[(inp_s >>  0) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >>  4) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >>  8) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >> 12) & 0xf];
    *buf_y-- = '.';
  }

  inp_s = u3r_short(len_w, a);

  while ( inp_s ) {
    *buf_y-- = u3s_dit_y[inp_s & 0xf];
    inp_s >>= 4;
  }

  *buf_y-- = 'x';
  *buf_y   = '0';
}

/* u3s_etch_ux(): atom to @ux.
*/
u3_atom
u3s_etch_ux(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'x', '0');
  }

  c3_w     sep_w = u3r_met(4, a) - 1;                //  number of separators
  c3_w     las_w = u3r_met(2, u3r_short(sep_w, a));  //  digits before separator
  c3_w     len_w = 2 + las_w + (sep_w * 5);          //  output bytes
  u3i_slab sab_u;
  u3i_slab_bare(&sab_u, 3, len_w);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_ux_bytes(a, sep_w, sab_u.buf_y + len_w - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_ux_c(): atom to @ux, as a malloc'd c string.
*/
size_t
u3s_etch_ux_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0x0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   sep_w = u3r_met(4, a) - 1;
  c3_w   las_w = u3r_met(2, u3r_short(sep_w, a));
  size_t len_i = 2 + las_w + (sep_w * 5);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_ux_bytes(a, sep_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

//  uint div+ceil non-zero
//
#define _divc_nz(x, y)  (((x) + ((y) - 1)) / (y))

/* _cs_etch_uv_size(): output length in @uv (and aligned bits).
*/
static inline size_t
_cs_etch_uv_size(u3_atom a, c3_w* out_w)
{
  c3_w met_w = u3r_met(0, a);
  c3_w sep_w = _divc_nz(met_w, 25) - 1;  //  number of separators
  c3_w max_w = sep_w * 25;
  c3_w end_w = 0;
  u3r_chop(0, max_w, 25, 0, &end_w, a);

  c3_w bit_w = c3_bits_word(end_w);
  c3_w las_w = _divc_nz(bit_w, 5);       //  digits before separator

  *out_w = max_w;
  return 2 + las_w + (sep_w * 6);
}


/* _cs_etch_uv_bytes(): atom to @uv impl.
*/
static void
_cs_etch_uv_bytes(u3_atom a, c3_w max_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_w inp_w;

  for ( i_w = 0; i_w < max_w; i_w += 25 ) {
    inp_w = 0;
    u3r_chop(0, i_w, 25, 0, &inp_w, a);

    *buf_y-- = u3s_dit_y[(inp_w >>  0) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >>  5) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 10) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 15) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 20) & 0x1f];
    *buf_y-- = '.';
  }

  inp_w = 0;
  u3r_chop(0, max_w, 25, 0, &inp_w, a);

  while ( inp_w ) {
    *buf_y-- = u3s_dit_y[inp_w & 0x1f];
    inp_w >>= 5;
  }

  *buf_y-- = 'v';
  *buf_y   = '0';
}

/* u3s_etch_uv(): atom to @uv.
*/
u3_atom
u3s_etch_uv(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'v', '0');
  }

  u3i_slab sab_u;
  c3_w     max_w;
  size_t   len_i = _cs_etch_uv_size(a, &max_w);

  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_uv_bytes(a, max_w, sab_u.buf_y + len_i - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_uv_c(): atom to @uv, as a malloc'd c string.
*/
size_t
u3s_etch_uv_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0v0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   max_w;
  size_t len_i = _cs_etch_uv_size(a, &max_w);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_uv_bytes(a, max_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

/* _cs_etch_uw_size(): output length in @uw (and aligned bits).
*/
static inline size_t
_cs_etch_uw_size(u3_atom a, c3_w* out_w)
{
  c3_w met_w = u3r_met(0, a);
  c3_w sep_w = _divc_nz(met_w, 30) - 1;  //  number of separators
  c3_w max_w = sep_w * 30;
  c3_w end_w = 0;
  u3r_chop(0, max_w, 30, 0, &end_w, a);

  c3_w bit_w = c3_bits_word(end_w);
  c3_w las_w = _divc_nz(bit_w, 6);       //  digits before separator

  *out_w = max_w;
  return 2 + las_w + (sep_w * 6);
}

/* _cs_etch_uw_bytes(): atom to @uw impl.
*/
static void
_cs_etch_uw_bytes(u3_atom a, c3_w max_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_w inp_w;

  for ( i_w = 0; i_w < max_w; i_w += 30 ) {
    inp_w = 0;
    u3r_chop(0, i_w, 30, 0, &inp_w, a);

    *buf_y-- = u3s_dit_y[(inp_w >>  0) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >>  6) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 12) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 18) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 24) & 0x3f];
    *buf_y-- = '.';
  }

  inp_w = 0;
  u3r_chop(0, max_w, 30, 0, &inp_w, a);

  while ( inp_w ) {
    *buf_y-- = u3s_dit_y[inp_w & 0x3f];
    inp_w >>= 6;
  }

  *buf_y-- = 'w';
  *buf_y   = '0';
}

/* u3s_etch_uw(): atom to @uw.
*/
u3_atom
u3s_etch_uw(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'w', '0');
  }

  u3i_slab sab_u;
  c3_w     max_w;
  size_t   len_i = _cs_etch_uw_size(a, &max_w);

  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_uw_bytes(a, max_w, sab_u.buf_y + len_i - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_uw_c(): atom to @uw, as a malloc'd c string.
*/
size_t
u3s_etch_uw_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0w0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   max_w;
  size_t len_i = _cs_etch_uw_size(a, &max_w);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_uw_bytes(a, max_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

#undef _divc_nz

/* +dot
 */
static inline c3_o _cs_dot(c3_w* len_w, c3_y** byt_yp)
{
  if ( *len_w > 0 && **byt_yp == '.' ) {
    (*byt_yp)++;
    (*len_w)--;
    return c3y;
  }
  else {
    return c3n;
  }
}

#define arelower(a,b,c) (islower(a) && islower(b) && islower(c))

static inline c3_s _cs_parse_prefix(c3_w* len_w, c3_y** byt_yp) {

  c3_y a,b,c;
  c3_y* byt_y = *byt_yp;

  if ( *len_w < 3 ) {
    return -1;
  }

  a = *byt_y;
  b = *(byt_y + 1);
  c = *(byt_y + 2);

  if ( ! arelower(a,b,c) ) {
    return -1;
  }

  *byt_yp += 3;
  *len_w -= 3;

  return u3_po_find_prefix(a,b,c);
}

static inline c3_s _cs_parse_suffix(c3_w* len_w, c3_y** byt_yp) {

  c3_y a,b,c;
  c3_y* byt_y = *byt_yp;

  if ( *len_w < 3 ) {
    return -1;
  }

  a = *byt_y;
  b = *(byt_y + 1);
  c = *(byt_y + 2);

  if ( ! arelower(a,b,c) ) {
    return -1;
  }

  *byt_yp += 3;
  *len_w -= 3;

  return u3_po_find_suffix(a,b,c);
}

#undef arelower

/* u3s_sift_p_bytes: parse @p impl.
 */
u3_weak
u3s_sift_p_bytes(c3_w len_w, c3_y* byt_y)
{
  c3_d pun_d;

  c3_s suf_s;
  c3_s puf_s;

  if ( !len_w || *byt_y != '~') {
    return u3_none;
  }

  len_w--;
  byt_y++;

  suf_s = _cs_parse_suffix(&len_w, &byt_y);

  // A galaxy
  //
  if ( !len_w && suf_s <= 0xff) {
    return (u3_atom) suf_s;
  }

  if ( !len_w ) {
    return u3_none;
  }

  // Rewind to match a star
  //
  len_w += 3;
  byt_y -= 3;

  puf_s = _cs_parse_prefix(&len_w, &byt_y);

  if ( puf_s > 0xff) {
    return u3_none;
  }

  suf_s = _cs_parse_suffix(&len_w, &byt_y);

  if ( suf_s > 0xff ) {
    return u3_none;
  }

  pun_d = (puf_s << 8 ) + suf_s;

  // A star, disallow ~doz for prefix
  //
  if ( !len_w && puf_s > 0 ) {
    return (u3_atom) pun_d;
  }

  // +hef
  if ( !pun_d ) {
    return u3_none;
  }

  // At least a planet
  //
  if ( len_w < 7 ) {
    return u3_none;
  }

  size_t hak = 1;

  // Parse up to 3 head words (64-bit)
  //
  while ( len_w && hak < 4) {

    if ( *byt_y != '-') {
      return u3_none;
    }

    byt_y++;
    len_w--;

    puf_s = _cs_parse_prefix(&len_w, &byt_y);

    if ( puf_s > 0xff ) {

      // --, end of head
      //
      if ( *byt_y == '-' ) {
        break;
      }

      else {
        return u3_none;
      }
    }

    suf_s = _cs_parse_suffix(&len_w, &byt_y);

    if ( suf_s > 0xff ) {
      return u3_none;
    }

    pun_d <<= 16;
    pun_d += (puf_s << 8) + suf_s;

    hak++;
  }

  if ( !len_w ) {

      if ( c3y == u3a_is_cat(pun_d) ) {
          return (u3_atom) u3qe_fynd_ob(pun_d);
      }
      else {
          u3_atom pun = u3i_chub(pun_d);
          u3_atom sun = u3qe_fynd_ob(pun);

          u3z(pun);
          return sun;
      }
  }

  // Parse a big address in quadruples
  //
  mpz_t pun_mp;
  mpz_init2(pun_mp, 128);

  mpz_set_ui(pun_mp, pun_d);
  pun_d = 0;

  hak = 0;

  // Rewind to separating --
  //
  byt_y -= 1;
  len_w += 1;

  // Parse in 64-bit chunks
  //
  while ( len_w ) {

    if ( *byt_y != '-') {
      goto sift_p_fail;
    }

    byt_y++;
    len_w--;

    if ( 0 == (hak % 4) ) {

      // Separated by --
      //
      if ( *byt_y != '-' || len_w < 7) {
        goto sift_p_fail;
      }

      byt_y++;
      len_w--;
    }

    puf_s = _cs_parse_prefix(&len_w, &byt_y);

    if ( puf_s > 0xff ) {
      goto sift_p_fail;
    }

    suf_s = _cs_parse_suffix(&len_w, &byt_y);

    if ( suf_s > 0xff ) {
      goto sift_p_fail;
    }

    pun_d <<= 16;
    pun_d += (puf_s << 8) + suf_s;

    hak++;

    if ( hak == 4 ) {
      mpz_mul_2exp(pun_mp, pun_mp, 64);
      mpz_add_ui(pun_mp, pun_mp, pun_d);

      pun_d = 0;
      hak = 0;
    }
  }

  // Number of words in the tail
  // must be a multiple of four
  //
  if ( hak ) {
    goto sift_p_fail;
  }

  if ( len_w ) {
sift_p_fail:

  mpz_clear(pun_mp);
  return u3_none;
  }

  u3_atom pun = u3i_mp(pun_mp);
  u3_atom sun = u3qe_fynd_ob(pun);

  u3z(pun);
  return sun;
}

/* u3s_sift_p: parse @p.
*/
u3_weak
u3s_sift_p(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  //
  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_p_bytes(len_w, byt_y);
}

#define DIGIT(a) ( ((a) >= '0') && ((a) <= '9') )

/* +two: parse a maximum 2 digit decimal number, greater than 0.
 */
static inline c3_o _cs_two(c3_s* num, c3_w* len_w, c3_y** byt_yp)
{

  if ( !(*len_w) || !DIGIT(**byt_yp) || **byt_yp == '0' ) {
    return c3n;
  }

  *num = **byt_yp - '0';

  (*byt_yp)++;
  (*len_w)--;

  if ( *len_w && DIGIT(**byt_yp)) {
    *num *= 10;
    *num += **byt_yp - '0';

    (*byt_yp)++;
    (*len_w)--;
  }

  return c3y;
}

/* +duo: parse a maximum 2 digit decimal number,
 * allowing for leading 0.
 */
static inline c3_o _cs_duo(c3_s* num, c3_w* len_w, c3_y** byt_yp)
{

  if ( !(*len_w) || !DIGIT(**byt_yp) ) {
    return c3n;
  }

  *num = **byt_yp - '0';
  (*byt_yp)++;
  (*len_w)--;

  if ( *len_w && DIGIT(**byt_yp)) {
    *num *= 10;
    *num += **byt_yp - '0';
    (*byt_yp)++;
    (*len_w)--;
  }

  return c3y;
}

/* _cs_dex_val: char to decimal digit.
 */
static inline c3_s _cs_dex_val(c3_y dex) {

  if ( dex > '9' ) {
    return -1;
  }
  else {
    return dex  - '0';
  }
}

/* _cs_hex_val: char to hexadecimal digit.
 */
static inline c3_s _cs_hex_val(c3_y hex) {

  if ( hex > '9' ) {
    if ( hex < 'a' ) {
      return -1;
    }
    // hex >= 'a'
    else {
      return (hex - 'a') + 10;
    }
  }
  // hex <= '9'
  else {
    return hex - '0';
  }
}

/* +yelp
 */
static inline c3_o _cs_yelp_mp(mpz_t yer_mp)
{

  c3_o res_o;

  if ( mpz_divisible_ui_p(yer_mp, 4) ) {

    if ( mpz_divisible_ui_p(yer_mp, 100) ) {

      if ( mpz_divisible_ui_p(yer_mp, 400) ) {
        res_o = c3y;
      }
      else {
        res_o = c3n;
      }
    }

    else {
      res_o = c3y;
    }
  }
  else {
    res_o = c3n;
  }

  return res_o;
}

/* ++yelq:when:so
 */
static inline c3_o _cs_yelq_mp(c3_o ad_o, mpz_t yer_mp) {

  if ( _(ad_o) ) {
    return _cs_yelp_mp(yer_mp);
  }
  else {
    c3_o res_o;

    mpz_t ber_mp;
    mpz_init(ber_mp);

    mpz_sub_ui(ber_mp, yer_mp, 1);

    res_o = _cs_yelp_mp(ber_mp);

    mpz_clear(ber_mp);
    return res_o;
  }
}
/* +yawn: days since the beginning.
 */
static inline void _cs_yawn(mpz_t days_mp, struct _cald* cal)
{
  c3_s* cah;

  mpz_init2(days_mp, 128);

  if ( _(_cs_yelp_mp(cal->yer_mp)) ) {
    cah = _cs_moy_yo;
  }
  else {
    cah = _cs_moh_yo;
  }

  cal->day_s--;
  cal->mot_s--;

  // Elapsed days
  //
  mpz_add_ui(days_mp, days_mp, cal->day_s);

  // Elapsed days in months
  //
  for ( size_t mot = 0; mot < cal->mot_s; mot++ ) {
    mpz_add_ui(days_mp, days_mp, cah[mot]);
  }

  // Elapsed days in years
  //
  while ( mpz_cmp_ui(cal->yer_mp, 0) > 0) {

    // Not divisible by 4
    //
    if ( ! mpz_divisible_ui_p(cal->yer_mp, 4) ) {

      mpz_sub_ui(cal->yer_mp, cal->yer_mp, 1);

      if ( _(_cs_yelp_mp(cal->yer_mp)) ) {
        mpz_add_ui(days_mp, days_mp, 366);
      }
      else {
        mpz_add_ui(days_mp, days_mp, 365);
      }
    }

    // Divisible by 4
    //
    else {

      // Not divisible by 100
      //
      if ( ! mpz_divisible_ui_p(cal->yer_mp, 100) ) {

        mpz_sub_ui(cal->yer_mp, cal->yer_mp, 4);

        if ( _(_cs_yelp_mp(cal->yer_mp)) ) {
          mpz_add_ui(days_mp, days_mp, 1 + 365*4);
        }
        else {
          mpz_add_ui(days_mp, days_mp, 365*4);
        }
      }

      // Divisible by 4 & 100
      //
      else {

        // Not divisible by 400
        //
        if ( ! mpz_divisible_ui_p(cal->yer_mp, 400) ) {
          mpz_sub_ui(cal->yer_mp, cal->yer_mp, 100);

          if ( _(_cs_yelp_mp(cal->yer_mp)) ) {
            mpz_add_ui(days_mp, days_mp, 1 + 365*100 + 24);
          }
          else {
            mpz_add_ui(days_mp, days_mp, 365*100 + 24);
          }
        }
        // Divisible by 4 & 100 & 400,
        // finish the calculation
        //
        else {
          mpz_tdiv_q_ui(cal->yer_mp, cal->yer_mp, 400);
          mpz_addmul_ui(days_mp, cal->yer_mp, 1 + (365*100+24)*4);
          break;
        }
      }
    }
  }
}

#define HEXDIGIT(a) ( ((a) >= '0' && (a) <= '9') || ((a) >= 'a' && (a) <= 'f') )

/* u3s_sift_da_bytes: parse @da impl.
 */
u3_weak
u3s_sift_da_bytes(c3_w len_w, c3_y* byt_y)
{
  struct _cald cal;

  if ( !len_w ) return u3_none;

  // ++ slaw %da
  //

  // The shortest date is ~1.1.1, 6 bytes
  if ( *byt_y != '~' || len_w < 6) return u3_none;

  byt_y++;
  len_w--;

  // Parse the year
  //
  if ( DIGIT(*byt_y) && *byt_y != '0' ) {

    mpz_init(cal.yer_mp);

    mpz_add_ui(cal.yer_mp, cal.yer_mp, *byt_y - '0');

    len_w--;
    byt_y++;

    while ( DIGIT(*byt_y) && len_w > 0) {

      mpz_mul_ui(cal.yer_mp, cal.yer_mp, 10);
      mpz_add_ui(cal.yer_mp, cal.yer_mp, *byt_y - '0');

      len_w--;
      byt_y++;
    }

  }

  else {
    return u3_none;
  }

  if ( !len_w ) {
    goto sift_da_fail;
  }

  // Optional following hep to indicate a BC year
  //
  if ( *byt_y == '-' ) {

    cal.yad_o = c3n;

    len_w--;
    byt_y++;
  }
  else {
    cal.yad_o = c3y;
  }

  // +veal: check whether the year is in proper range
  // For AD years: from 1 AD to the future
  // For BC years: from 1 BC until JES_YO+1 BC
  //
  if ( mpz_cmp_ui(cal.yer_mp, 0) == 0 ||
      (!_(cal.yad_o) && mpz_cmp_ui(cal.yer_mp, JES_YO+1) > 0 ) ) {
    goto sift_da_fail;
  }

  // We need at least .m.d
  //
  if ( len_w < 4 ) {
    goto sift_da_fail;
  }

  // Parse .month
  //
  cal.mot_s = 0;

  if ( !_(_cs_dot(&len_w, &byt_y)) ||
       !_(_cs_two(&cal.mot_s, &len_w, &byt_y)) ) {
    goto sift_da_fail;
  }

  if ( cal.mot_s > 12 ) {
    goto sift_da_fail;
  }

  // Parse .day
  //
  cal.day_s = 0;

  if ( !_(_cs_dot(&len_w, &byt_y)) ||
       !_(_cs_two(&cal.day_s, &len_w, &byt_y)) ){
    goto sift_da_fail;
  }


  // Validate the day
  //
  c3_s mob;

  // Leap year
  //
  if ( _(_cs_yelq_mp(cal.yad_o, cal.yer_mp)) ) {
    mob = _cs_moy_yo[cal.mot_s-1];
  }
  else {
    mob = _cs_moh_yo[cal.mot_s-1];
  }

  if ( cal.day_s > mob ) {
    goto sift_da_fail;
  }


  /* hor.mit.sec..fan
   */
  c3_s hor_s = 0;
  c3_s mit_s = 0;
  c3_s sec_s = 0;
  c3_d fan_d = 0;

  // Parse the time ..hh.mm.ss
  // This requires minimum of 10 bytes
  //
  if ( len_w >= 10 ) {

    if ( *byt_y != '.' || *(byt_y+1) != '.') {
      goto sift_da_fail;
    }

    len_w -= 2;
    byt_y += 2;

    // Parse hour
    //
    if ( !_(_cs_duo(&hor_s, &len_w, &byt_y)) ) {
      goto sift_da_fail;
    }

    if ( hor_s > 23 ) {
      goto sift_da_fail;
    }

    // Parse .minutes
    //
    if ( !_(_cs_dot(&len_w, &byt_y)) ||
         !_(_cs_duo(&mit_s, &len_w, &byt_y)) ) {
      goto sift_da_fail;
    }

    if ( mit_s > 59 ) {
      goto sift_da_fail;
    }

    // Parse .seconds
    //
    if ( !_(_cs_dot(&len_w, &byt_y)) ||
         !_(_cs_duo(&sec_s, &len_w, &byt_y)) ) {
      goto sift_da_fail;
    }

    if ( sec_s > 59 ) {
      goto sift_da_fail;
    }

    // Parse ..fractional, at least ..cafe
    //
    if ( len_w >= 6 ) {

      if ( *byt_y != '.' ) {
        goto sift_da_fail;
      }

      byt_y++;
      len_w--;

      size_t muc = 4;

      while ( len_w >= 5 && muc > 0 ) {

        if ( *byt_y != '.' ) {
          goto sift_da_fail;
        }

        if ( !( HEXDIGIT(*(byt_y+1)) &&
              HEXDIGIT(*(byt_y+2)) &&
              HEXDIGIT(*(byt_y+3)) &&
              HEXDIGIT(*(byt_y+4)) ) ) {
          goto sift_da_fail;
        }

        fan_d += (c3_d)((_cs_hex_val(*(byt_y+1)) << 12) +
                  (_cs_hex_val(*(byt_y+2)) << 8) +
                  (_cs_hex_val(*(byt_y+3)) << 4) +
                  (_cs_hex_val(*(byt_y+4))) ) << ((muc-1)*16);
        muc--;
        len_w -= 5;
        byt_y += 5;

      }

    }
  }

  if ( len_w != 0 ) {
sift_da_fail:
    mpz_clear(cal.yer_mp);
    return u3_none;
  }

  //
  // ++ year, date to @da
  //

  // year to absolute year
  //
  if ( _(cal.yad_o) ) {
    mpz_add_ui(cal.yer_mp, cal.yer_mp, JES_YO);
  }

  else {
    mpz_ui_sub(cal.yer_mp, JES_YO+1, cal.yer_mp);
  }

  mpz_t days_mp;

  _cs_yawn(days_mp, &cal);

  // day_mp is now expressed in seconds
  //
  mpz_mul_ui(days_mp, days_mp, DAY_YO);

  if ( hor_s ) {
    mpz_add_ui(days_mp, days_mp, hor_s*HOR_YO);
  }

  if ( mit_s ) {
    mpz_add_ui(days_mp, days_mp, mit_s*MIT_YO);
  }

  if ( sec_s ) {
    mpz_add_ui(days_mp, days_mp, sec_s);
  }

  mpz_mul_2exp(days_mp, days_mp, 64);
  mpz_add_ui(days_mp, days_mp, fan_d);

  mpz_clear(cal.yer_mp);
  return u3i_mp(days_mp);
}

#undef HEXDIGIT

/* u3s_sift_da: parse @da.
*/
u3_weak
u3s_sift_da(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_da_bytes(len_w, byt_y);
}

#define BLOCK(a) (  ('.' == (a)[0]) \
                 && DIGIT(a[1])     \
                 && DIGIT(a[2])     \
                 && DIGIT(a[3])     )

/* u3s_sift_ud_bytes: parse @ud
*/
u3_weak
u3s_sift_ud_bytes(c3_w len_w, c3_y* byt_y)
{
  c3_y num_y = len_w % 4;  //  leading digits length
  c3_s val_s = 0;          //  leading digits value

  //  +ape:ag: just 0
  //
  if ( !len_w )        return u3_none;
  if ( '0' == *byt_y ) return ( 1 == len_w ) ? (u3_noun)0 : u3_none;

  //  +ted:ab: leading nonzero (checked above), plus up to 2 digits
  //
#define NEXT()  do {                          \
    if ( !DIGIT(*byt_y) ) return u3_none;     \
    val_s *= 10;                              \
    val_s += *byt_y++ - '0';                  \
  } while (0)

  switch ( num_y ) {
    case 3: NEXT();
    case 2: NEXT();
    case 1: NEXT(); break;
    case 0: return u3_none;
  }

#undef NEXT

  len_w -= num_y;

  //  +tid:ab: dot-prefixed 3-digit blocks
  //
  //    avoid gmp allocation if possible
  //      - 19 decimal digits fit in 64 bits
  //      - 18 digits is 24 bytes with separators
  //
  if (  ((1 == num_y) && (24 >= len_w))
     || (20 >= len_w) )
  {
    c3_d val_d = val_s;

    while ( len_w ) {
      if ( !BLOCK(byt_y) ) return u3_none;

      byt_y++;

      val_d *= 10;
      val_d += *byt_y++ - '0';
      val_d *= 10;
      val_d += *byt_y++ - '0';
      val_d *= 10;
      val_d += *byt_y++ - '0';

      len_w -= 4;
    }

    return u3i_chub(val_d);
  }

  {
    //  avoid gmp realloc if possible
    //
    mpz_t a_mp;
    {
      c3_d bit_d = (c3_d)(len_w / 4) * 10;
      mpz_init2(a_mp, (c3_w)c3_min(bit_d, UINT32_MAX));
      mpz_set_ui(a_mp, val_s);
    }

    while ( len_w ) {
      if ( !BLOCK(byt_y) ) {
        mpz_clear(a_mp);
        return u3_none;
      }

      byt_y++;

      val_s  = *byt_y++ - '0';
      val_s *= 10;
      val_s += *byt_y++ - '0';
      val_s *= 10;
      val_s += *byt_y++ - '0';

      mpz_mul_ui(a_mp, a_mp, 1000);
      mpz_add_ui(a_mp, a_mp, val_s);

      len_w -= 4;
    }

    return u3i_mp(a_mp);
  }
}

#undef BLOCK

/* u3s_sift_ud: parse @ud.
*/
u3_weak
u3s_sift_ud(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_ud_bytes(len_w, byt_y);
}

#define PFIXD(a,b) { \
  if ( len_w < 3) { \
    return u3_none; \
  } \
  if ( !(*byt_y == a && *(byt_y+1) == b) ) { \
    return u3_none; \
  } \
  len_w -= 2; \
  byt_y += 2; \
}

/* u3s_sift_ui_bytes: parse @ui.
 */
u3_weak
u3s_sift_ui_bytes(c3_w len_w, c3_y* byt_y)
{

  PFIXD('0', 'i');

  // Parse 0i0
  //
  if ( *byt_y == '0' ) {
    if ( len_w > 1 ) {
      return u3_none;
    }
    else {
      return (u3_noun)0;
    }
  }

  c3_d val_d = 0;

  // Avoid gmp allocation if possible
  //  - 19 decimal digits fit in 64 bits
  //
  if ( len_w <= 19 ) {

    c3_s dit_s;

    while ( len_w > 0 ) {

      dit_s = _cs_dex_val(*byt_y);

      if ( dit_s > 9 ) {
        return u3_none;
      }

      val_d *= 10;
      val_d += dit_s;

      byt_y++;
      len_w--;
    }

    return u3i_chub(val_d);
  }

  else {
    mpz_t a_mp;
    mpz_t bas_mp;

    // avoid gmp realloc if possible
    //
    {
      c3_d bit_d = len_w/3 * 10;
      mpz_init2(a_mp, (c3_w)c3_min(bit_d, UINT32_MAX));

      mpz_init(bas_mp);
      mpz_ui_pow_ui(bas_mp, 10, 19);
    }

    val_d = 0;
    c3_s hak_s = 0;

    while ( len_w ) {

      if ( ! DIGIT(*byt_y) ) {

        mpz_clear(bas_mp);
        mpz_clear(a_mp);
        return u3_none;
      }

      val_d *= 10;
      val_d += *byt_y++ - '0';

      len_w--;
      hak_s++;

      if ( hak_s == 19) {
        mpz_mul(a_mp, a_mp, bas_mp);
        mpz_add_ui(a_mp, a_mp, val_d);

        val_d = 0;
        hak_s = 0;
      }

    }

    if ( hak_s ) {
        mpz_ui_pow_ui(bas_mp, 10, hak_s);
        mpz_mul(a_mp, a_mp, bas_mp);
        mpz_add_ui(a_mp, a_mp, val_d);
    }

    mpz_clear(bas_mp);
    return u3i_mp(a_mp);
  }
}

#undef DIGIT

/* u3s_sift_ui: parse @ui.
 */
u3_weak
u3s_sift_ui(u3_noun a)
{

  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
    byt_y = (c3_y*)&a;
  }
  else{
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_ui_bytes(len_w, byt_y);
}


/* u3s_sift_ux_bytes: parse @ux impl.
 */
u3_weak
u3s_sift_ux_bytes(c3_w len_w, c3_y* byt_y)
{

  if ( ! len_w ) {
    return u3_none;
  }

  // Parse the 0x prefix
  //
  PFIXD('0', 'x');

  if ( ! len_w ) {
    return u3_none;
  }

  // Parse 0x0
  //
  if ( *byt_y == '0' ) {
    if ( len_w > 1 ) {
      return u3_none;
    }
    else {
      return (u3_noun)0;
    }
  }

  // Parse a small 64-bit hex number
  //
  c3_d val_d = 0;
  c3_s dit_s = 0;

  // Parse the head
  //
  for ( size_t i = 0; i < 4; i++ ) {

    if ( ! len_w ) {
      break;
    }

    dit_s = _cs_hex_val(*byt_y);

    if ( dit_s < 16 ) {
      val_d <<= 4;
      val_d += dit_s;
    }
    else {
      break;
    }

    byt_y++;
    len_w--;
  }

  // Parse a list of dog followed by
  // a quadruple of hex digits
  //
  size_t cuk = 0;

  while ( len_w && cuk < 3) {

    if ( ! _(_cs_dot(&len_w, &byt_y)) ) {
      return u3_none;
    }

    for ( size_t i = 0; i < 4; i++ ) {

      if ( ! len_w ) {
        return u3_none;
      }

      dit_s = _cs_hex_val(*byt_y);

      if ( dit_s < 16 ) {
        val_d <<= 4;
        val_d += dit_s;
      }
      else {
        return u3_none;
      }

      byt_y++;
      len_w--;
    }
    cuk++;
  }

  if ( !len_w ) {
    return u3i_chub(val_d);
  }

  // Parse a big hex
  //
  else {
    mpz_t a_mp;
    mpz_init2(a_mp, 128);
    mpz_set_ui(a_mp, val_d);

    val_d = 0;

    // Parse a list of dog followed by
    // a quadruple of hex digits
    //
    cuk = 0;

    while ( len_w ) {

      if ( ! _(_cs_dot(&len_w, &byt_y)) ) {
        goto sift_ux_fail;
      }

      for ( size_t i = 0; i < 4; i++ ) {

        if ( ! len_w ) {
          goto sift_ux_fail;
        }

        dit_s = _cs_hex_val(*byt_y);

        if ( dit_s < 16 ) {
          val_d <<= 4;
          val_d += dit_s;
        }
        else {
          goto sift_ux_fail;
        }

        byt_y++;
        len_w--;
      }

      cuk++;

      // Read 4 chunks
      //
      if ( cuk == 4 ) {
        mpz_mul_2exp(a_mp, a_mp, cuk*16);
        mpz_add_ui(a_mp, a_mp, val_d);

        val_d = 0;
        cuk = 0;
      }
    }

    if ( cuk ) {
      mpz_mul_2exp(a_mp, a_mp, cuk*16);
      mpz_add_ui(a_mp, a_mp, val_d);
    }

    if ( len_w ) {
sift_ux_fail:
      mpz_clear(a_mp);
      return u3_none;
    }

    return u3i_mp(a_mp);
  }

}

/* u3s_sift_ux: parse @ux.
 */
u3_weak
u3s_sift_ux(u3_noun a)
{

  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
    byt_y = (c3_y*)&a;
  }

  else{
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_ux_bytes(len_w, byt_y);
}

#undef PFIXD
