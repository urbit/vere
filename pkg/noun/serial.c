/// @file

#include "serial.h"

#include <errno.h>
#include <fcntl.h>

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets/k.h"
#include "jets/q.h"
#include "options.h"
#include "retrieve.h"
#include "serial.h"
#include "ur/ur.h"
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
static inline void
_cs_jam_fib_grow(struct _cs_jam_fib* fib_u, c3_w mor_w)
{
  c3_w wan_w = fib_u->bit_w + mor_w;

  // check for c3_w overflow
  //
  if ( wan_w < mor_w ) {
    u3m_bail(c3__fail);
    return;
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
static inline void
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
    //  for bob atoms, mmap the blob file directly to avoid loom allocation.
    //  for normal atoms, use u3r_met as before.
    //
    c3_o         bob_o = u3a_is_bob(a);
    c3_d         byt_d = 0;
    const c3_y*  byt_y = 0;

    c3_w   a_w;
    if ( c3y == bob_o ) {
      byt_y = u3r_blob_map(a, &byt_d);
      if ( !byt_y ) {
        u3m_bail(c3__fail);
        return;
      }
      //  compute bit-length (met) from mmap'd bytes; strip trailing zero bytes
      //  and find the MSB position of the last non-zero byte.
      //
      {
        c3_d pos_d = byt_d;
        while ( pos_d > 0 && 0 == byt_y[pos_d - 1] ) {
          pos_d--;
        }
        if ( 0 == pos_d ) {
          //  blob is all zeros → atom value is 0; treat as zero atom
          u3r_blob_unmap(byt_y, byt_d);
          _cs_jam_fib_chop(fib_u, 1, 1);
          return;
        }
        c3_y top_y = byt_y[pos_d - 1];
        c3_y clz_y = (c3_y)(__builtin_clz((unsigned int)top_y) - 24);
        a_w = (c3_w)((pos_d - 1) * 8 + (c3_d)(8 - clz_y));
      }
    }
    else {
      a_w = u3r_met(0, a);
    }

    c3_w   b_w = c3_bits_word(a_w);
    c3_w bit_w = fib_u->bit_w;

    //  amortize overflow checks and reallocation
    //
    {
      c3_w met_w = a_w + (2 * b_w);

      if ( a_w > (c3_w_max - 64) ) {
        if ( byt_y ) u3r_blob_unmap(byt_y, byt_d);
        u3m_bail(c3__fail);
        return;
      }

      _cs_jam_fib_grow(fib_u, met_w);
      fib_u->bit_w += met_w;
    }

    {
#ifndef VERE64
      c3_w  src_w[2];
#else
      c3_w  src_w[1];
#endif

      c3_w* buf_w = fib_u->sab_u->buf_w;

      //  _cs_jam_fib_chop(fib_u, b_w+1, 1 << b_w);
      //
      {
#ifndef VERE64
        c3_d dat_d = (c3_d)1 << b_w;
        src_w[0]   = (c3_w)dat_d;
        src_w[1]   = dat_d >> 32;
        u3r_chop_words(0, 0, b_w + 1, bit_w, buf_w, 2, src_w);
#else
        src_w[0] = (c3_d)1 << b_w;
        u3r_chop_words(0, 0, b_w + 1, bit_w, buf_w, 1, src_w);
#endif

        bit_w += b_w + 1;
      }

      //  _cs_jam_fib_chop(fib_u, b_w-1, a_w);
      //
      {
        src_w[0] = a_w;
        u3r_chop_words(0, 0, b_w - 1, bit_w, buf_w, 1, src_w);
        bit_w += b_w - 1;
      }

      //  _cs_jam_fib_chop(fib_u, a_w, a);
      //
      if ( byt_y ) {
        //  write bob atom bytes directly from mmap, no loom allocation
        //
        c3_w len_w = (c3_w)((byt_d + sizeof(c3_w) - 1) / sizeof(c3_w));
        u3r_chop_words(0, 0, a_w, bit_w, buf_w, len_w, (const c3_w*)byt_y);
        u3r_blob_unmap(byt_y, byt_d);
      }
      else {
        u3r_chop(0, 0, a_w, bit_w, buf_w, a);
      }
    }
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
  return ( ((c3_d)u3a_direct_max) >= a_d ) ? a_d : u3i_chubs(1, &a_d);
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
  else if ( c3y == u3a_is_bob(a) ) {
    //  bob atom: mmap the blob file and write bytes directly into the bitstream
    //
    c3_d        len_d;
    const c3_y* byt_y = u3r_blob_map(a, &len_d);
    if ( byt_y ) {
      ur_bsw_atom_bytes(rit_u, (c3_d)met_w, (c3_y*)byt_y);
      u3r_blob_unmap(byt_y, len_d);
    }
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
  //  for bob atoms, use the blob met to avoid materializing the large atom;
  //  met_w must fit in 32 bits here (jam uses c3_w for bit lengths)
  //
  c3_w         met_w = ( c3y == u3a_is_bob(a) )
                     ? (c3_w)u3r_blob_met(a)
                     : u3r_met(0, a);

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
**
**   when [snk_u] is set, atoms larger than [thr_d] bytes are streamed
**   through it in chunks instead of being allocated on the loom; the
**   sink's don_f supplies the decoded atom (typically a bob atom).
*/
static inline ur_cue_res_e
_cs_cue_xeno_next(u3a_pile*    pil_u,
                  ur_bsr_t*    red_u,
                  ur_dictn_t* dic_u,
                  c3_d        thr_d,
                  u3s_bsink*  snk_u,
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
        //  XX: not 63?
        else if ( 62 < len_d ) {
          return ur_cue_meme;
        }
        else {
          c3_d bak_d = ur_bsr64_any(red_u, len_d);
          c3_w bak_w;

          if ( !ur_dictn_get(rot_u, dic_u, bak_d, &bak_w) ) {
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

        if ( (u3a_word_bits-1) >= len_d ) {
          *out = (u3_noun)ur_bsrn_any(red_u, len_d);
        }
        else if ( snk_u && (((len_d + 0x7) >> 3) > thr_d) ) {
          //  stream the atom's bytes through the sink, off-loom, in
          //  byte-multiple chunks (so they concatenate); the final
          //  chunk carries the bit remainder
          //
          if ( c3n == snk_u->opn_f(snk_u->ptr_v) ) {
            return ur_cue_gone;
          }

          {
            static const c3_d chk_d = ((c3_d)1 << 23);  //  8M bits = 1MB
            c3_y* buf_y = c3_malloc(chk_d >> 3);
            c3_d  rem_d = len_d;
            c3_o  oky_o = c3y;

            while ( rem_d ) {
              c3_d lim_d = c3_min(rem_d, chk_d);
              ur_bsr_bytes_any(red_u, lim_d, buf_y);
              if ( c3n == snk_u->wri_f(snk_u->ptr_v, buf_y,
                                       (c3_z)((lim_d + 0x7) >> 3)) )
              {
                oky_o = c3n;
                break;
              }
              rem_d -= lim_d;
            }

            c3_free(buf_y);

            if ( c3n == oky_o ) {
              return ur_cue_gone;
            }
          }

          {
            u3_weak bob = snk_u->don_f(snk_u->ptr_v);
            if ( u3_none == bob ) {
              return ur_cue_gone;
            }
            *out = bob;
          }
        }
        else {
          c3_d     byt_d = (len_d + 0x7) >> 3;
          u3i_slab sab_u;

          if ( c3_w_max < byt_d) {
            return ur_cue_meme;
          }
          else {
            u3i_slab_init(&sab_u, 3, byt_d);
            ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
            *out = u3i_slab_mint_bytes(&sab_u);
          }
        }

        ur_dictn_put(rot_u, dic_u, bit_d, *out);
        return ur_cue_good;
      }
    }
  }
}

struct _u3_cue_xeno {
  ur_dictn_t dic_u;
  c3_d       thr_d;  //  blob threshold in bytes (0 = disabled)
  u3s_bsink* snk_u;  //  byte sink for streamed large atoms
};

/* _cs_cue_xeno(): cue on-loom, with off-loom dictionary in handle.
*/
static u3_weak
_cs_cue_xeno(u3_cue_xeno* sil_u,
             c3_d         len_d,
             const c3_y*  byt_y)
{
  ur_bsr_t      red_u = {0};
  ur_dictn_t*  dic_u = &sil_u->dic_u;
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
  //  XX: not 63?
  else if ( 0x7ffffffffffffffULL < len_d ) {
    return c3n;
  }

  //  advance into stream
  //
  res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u,
                            sil_u->thr_d, sil_u->snk_u, &ref);

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
        res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u,
                                  sil_u->thr_d, sil_u->snk_u, &ref);
        fam_u = u3a_peek(&pil_u);
      }
      //  f is a tail-frame; pop the stack and continue
      //
      else {
        ur_root_t* rot_u = 0;

        ref   = u3nc(fam_u->ref, ref);
        ur_dictn_put(rot_u, dic_u, fam_u->bit_d, ref);
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

  sil_u = c3_calloc(sizeof(*sil_u));
  ur_dictn_grow((ur_root_t*)0, &sil_u->dic_u, pre_d, siz_d);

  return sil_u;
}

/* u3s_cue_xeno_blob(): install a byte sink on a cue_xeno handle.
*/
void
u3s_cue_xeno_blob(u3_cue_xeno* sil_u,
                  c3_d         thr_d,
                  u3s_bsink*   snk_u)
{
  sil_u->thr_d = thr_d;
  sil_u->snk_u = snk_u;
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
  ur_dictn_wipe(&sil_u->dic_u);
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

        //  XX: not 63?
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

        if ( (u3a_word_bits-1) >= len_d ) {
          vat = (u3_noun)ur_bsrn_any(red_u, len_d);
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
  //  XX: not 63?
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
  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
    c3_w len_w = u3r_met(3, a);
    return u3s_cue_bytes((c3_d)len_w, (c3_y*)&a);
  }

  //  bob atom: mmap the backing file instead of dereferencing buf_w
  //  (which for a bob would yield seq_h).  The view stays live for
  //  the whole cue so the bitstream reader can scan freely.
  //
  if ( c3y == u3a_is_bob(a) ) {
    u3r_view vu_u;
    u3r_view_init(&vu_u, a);
    u3_noun res = u3s_cue_bytes((c3_d)vu_u.len_w, (c3_y*)vu_u.byt_y);
    u3r_view_done(&vu_u);
    return res;
  }

  {
    c3_w      len_w = u3r_met(3, a);
    u3a_atom* vat_u = u3a_to_ptr(a);
    return u3s_cue_bytes((c3_d)len_w, (c3_y*)vat_u->buf_w);
  }
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

  mpz_init2(b_mp, 10);

  if ( !mpz_size(a_mp) ) {
    *buf_y-- = '0';
  }
  else {
    while ( 1 ) {
      b_w = mpz_tdiv_qr_ui(a_mp, b_mp, a_mp, 1000);
      u3_assert( mpz_get_ui(b_mp) == b_w ); // XX

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

  u3_assert( buf_y >= hun_y ); // XX

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
u3s_etch_ud_smol(c3_d a_d, c3_y hun_y[26])
{
  c3_y*  buf_y = hun_y + 25;
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
    c3_y  hun_y[26];
    c3_y* buf_y = u3s_etch_ud_smol(a_d, hun_y);
    c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;
    return u3i_bytes(26 - dif_w, buf_y);
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
    c3_y  hun_y[26];

    buf_y = u3s_etch_ud_smol(a_d, hun_y);
    len_i = 26 - ((c3_p)buf_y - (c3_p)hun_y);

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

  mpz_clear(a_mp);

  *out_c = (c3_c*)buf_y;
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

  // XX: 64 what do 
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

  // XX: 64 what do
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

#define DIGIT(a) ( ((a) >= '0') && ((a) <= '9') )
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
      c3_d bit_d = (c3_d)(len_w / sizeof(c3_w)) * 10;
      mpz_init2(a_mp, (c3_w)c3_min(bit_d, c3_w_max));
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
#undef DIGIT

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
     u3_assert(c3n == u3a_is_bob(a));
     u3a_atom* vat_u = u3a_to_ptr(a);
     byt_y = (c3_y*)vat_u->buf_w;
   }

   return u3s_sift_ud_bytes(len_w, byt_y);
}

/*
**  Ram/Tap — reference-aware serialization
**
**  Ram extends jam's bit-encoding with a 2-bit fixed tag:
**    00 = normal atom   (mat-encoded value follows)
**    01 = blob ref      (mat-encoded mug, then mat-encoded seq)
**    10 = cell
**    11 = backref       (mat-encoded bit-position follows)
**
**  All tags are exactly 2 bits (LSB first).
**  Wire format: ["RAM\0" 4B][0x01 1B][ram_bits...]
**
**  Ram does NOT blobify atoms. Caller must ensure large atoms are
**  already bob atoms before calling u3s_ram_xeno().
*/

#define U3S_RAM_MAGIC  "\x52\x41\x4d\x00"   /* "RAM\0" */
#define U3S_RAM_VERSION  0x01

/* Ram 2-bit tag values (LSB first)
*/
#define U3S_RAM_TAG_ATOM  0   /* 00 */
#define U3S_RAM_TAG_BOB   1   /* 01 */
#define U3S_RAM_TAG_CELL  2   /* 10 */
#define U3S_RAM_TAG_BACK  3   /* 11 */

/* _ram_xeno_t: state for the ram encoding walk.
*/
typedef struct _ram_xeno_s {
  u3p(u3h_root) har_p;
  ur_bsw_t      rit_u;
} _ram_xeno_t;

/* _cs_ram_bsw_2tag(): write a 2-bit ram tag to the bitstream.
*/
static inline void
_cs_ram_bsw_2tag(ur_bsw_t* rit_u, c3_y tag_y)
{
  ur_bsw8(rit_u, 2, tag_y & 3);
}

/* _cs_ram_bsw_normal_atom(): encode a normal (non-bob) atom as tag 00 + mat.
*/
static inline void
_cs_ram_bsw_normal_atom(ur_bsw_t* rit_u, c3_w met_w, u3_atom a)
{
  //  write 2-bit tag 00
  //
  _cs_ram_bsw_2tag(rit_u, U3S_RAM_TAG_ATOM);

  if ( c3y == u3a_is_cat(a) ) {
    ur_bsw_mat64(rit_u, (c3_y)met_w, (c3_d)a);
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    c3_y*     byt_y = (c3_y*)vat_u->buf_w;
    ur_bsw_mat_bytes(rit_u, (c3_d)met_w, byt_y);
  }
}

/* _cs_ram_bsw_bob(): encode a bob atom as tag 01 + mat(mug) + mat(seq).
*/
static inline void
_cs_ram_bsw_bob(ur_bsw_t* rit_u, u3_atom a)
{
  c3_h mug_h = u3a_bob_mug(a);
  c3_h seq_h = u3a_bob_seq(a);

  //  write 2-bit tag 01
  //
  _cs_ram_bsw_2tag(rit_u, U3S_RAM_TAG_BOB);

  //  write mat(mug) and mat(seq)
  //
  ur_bsw_mat64(rit_u, u3r_met(0, (u3_atom)mug_h), (c3_d)mug_h);
  ur_bsw_mat64(rit_u, u3r_met(0, (u3_atom)seq_h), (c3_d)seq_h);
}

/* _cs_ram_bsw_back(): encode a backref as tag 11 + mat(bit-position).
*/
static inline void
_cs_ram_bsw_back(ur_bsw_t* rit_u, c3_w met_w, u3_atom a)
{
  c3_d bak_d = ( c3y == u3a_is_cat(a) )
             ? (c3_d)a
             : u3r_chub(0, a);

  //  write 2-bit tag 11
  //
  _cs_ram_bsw_2tag(rit_u, U3S_RAM_TAG_BACK);
  ur_bsw_mat64(rit_u, (c3_y)met_w, bak_d);
}

/* _cs_ram_xeno_atom(): encode atom (or backref) in ram bitstream.
*/
static void
_cs_ram_xeno_atom(u3_atom a, void* ptr_v)
{
  _ram_xeno_t* ram_u = ptr_v;
  ur_bsw_t*    rit_u = &(ram_u->rit_u);
  u3_weak        bak = u3h_git(ram_u->har_p, a);
  c3_o         bob_o = u3a_is_bob(a);
  //  for bob atoms, use the blob's true bit-length for backref comparison.
  //  for normal atoms, use u3r_met as before.
  //
  c3_w         met_w = (c3n == bob_o) ? u3r_met(0, a)
                                       : (c3_w)u3r_blob_met(a);

  if ( u3_none == bak ) {
    u3h_put(ram_u->har_p, a, _cs_coin_chub(rit_u->bits));
    if ( c3y == bob_o ) {
      _cs_ram_bsw_bob(rit_u, a);
    }
    else {
      _cs_ram_bsw_normal_atom(rit_u, met_w, a);
    }
  }
  else {
    c3_w bak_w = u3r_met(0, bak);

    if ( met_w <= bak_w ) {
      if ( c3y == bob_o ) {
        _cs_ram_bsw_bob(rit_u, a);
      }
      else {
        _cs_ram_bsw_normal_atom(rit_u, met_w, a);
      }
    }
    else {
      _cs_ram_bsw_back(rit_u, bak_w, bak);
    }
  }
}

/* _cs_ram_xeno_cell(): encode cell (or backref) in ram bitstream.
*/
static c3_o
_cs_ram_xeno_cell(u3_noun a, void* ptr_v)
{
  _ram_xeno_t* ram_u = ptr_v;
  ur_bsw_t*    rit_u = &(ram_u->rit_u);
  u3_weak        bak = u3h_git(ram_u->har_p, a);

  if ( u3_none == bak ) {
    u3h_put(ram_u->har_p, a, _cs_coin_chub(rit_u->bits));
    //  write 2-bit tag 10 (cell)
    //
    _cs_ram_bsw_2tag(rit_u, U3S_RAM_TAG_CELL);
    return c3y;
  }
  else {
    _cs_ram_bsw_back(rit_u, u3r_met(0, bak), bak);
    return c3n;
  }
}

/* u3s_ram_xeno(): ram with off-loom buffer (re-)allocation.
*/
c3_d
u3s_ram_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y)
{
  _ram_xeno_t ram_u = {0};
  ur_bsw_init(&ram_u.rit_u, ur_fib11, ur_fib12);
  ram_u.har_p = u3h_new();

  u3a_walk_fore(a, &ram_u, _cs_ram_xeno_atom, _cs_ram_xeno_cell);

  u3h_free(ram_u.har_p);

  {
    c3_d   raw_bytes_d;  //  byte count of raw ram bits
    c3_y*  raw_y;
    c3_d   out_d;
    c3_y*  out_y;

    //  ur_bsw_done() returns bit count, sets raw_bytes_d to byte count
    //
    ur_bsw_done(&ram_u.rit_u, &raw_bytes_d, &raw_y);

    //  prepend 5-byte header: "RAM\0" + version
    //
    out_d = 5 + raw_bytes_d;
    out_y = malloc(out_d);
    if ( !out_y ) {
      free(raw_y);
      *len_d = 0;
      return 0;
    }

    memcpy(out_y, U3S_RAM_MAGIC, 4);
    out_y[4] = U3S_RAM_VERSION;
    memcpy(out_y + 5, raw_y, raw_bytes_d);
    free(raw_y);

    *len_d = out_d;
    *byt_y = out_y;
    return out_d;
  }
}

/*
**  Tap — ram deserialization
*/

/* _tap_frame_t: stack frame for cell reconstruction.
*/
typedef struct _tap_frame_s {
  u3_weak ref;    //  taken head, or u3_none if still on head
  c3_d    bit_d;  //  bit position of this noun
} _tap_frame_t;

/* _cs_tap_xeno_next(): read next value from ram bitstream.
*/
static inline ur_cue_res_e
_cs_tap_xeno_next(u3a_pile*    pil_u,
                  ur_bsr_t*    red_u,
                  ur_dictn_t*  dic_u,
                  u3_noun*     out)
{
  ur_root_t* rot_u = 0;

  while ( 1 ) {
    c3_d  len_d;
    c3_d  bit_d = red_u->bits;
    c3_y  tag_y;
    ur_cue_res_e res_e;

    //  read 2-bit ram tag (LSB first)
    //
    tag_y = ur_bsr8_any(red_u, 2);

    switch ( tag_y ) {
      default:
        return ur_cue_gone;

      case U3S_RAM_TAG_CELL: {
        //  push a head-frame and continue reading head
        //
        _tap_frame_t* fam_u = u3a_push(pil_u);
        fam_u->ref   = u3_none;
        fam_u->bit_d = bit_d;
        continue;
      }

      case U3S_RAM_TAG_BACK: {
        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }
        else if ( 62 < len_d ) {
          return ur_cue_meme;
        }
        else {
          c3_d  bak_d = ur_bsr64_any(red_u, len_d);
          c3_w  bak_w;

          if ( !ur_dictn_get(rot_u, dic_u, bak_d, &bak_w) ) {
            return ur_cue_back;
          }

          *out = u3k((u3_noun)bak_w);
          return ur_cue_good;
        }
      }

      case U3S_RAM_TAG_ATOM: {
        //  read mat-encoded value (same as cue atom path)
        //
        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }

        if ( (u3a_word_bits - 1) >= len_d ) {
          *out = (u3_noun)ur_bsrn_any(red_u, len_d);
        }
        else {
          c3_d     byt_d = (len_d + 0x7) >> 3;
          u3i_slab sab_u;

          if ( c3_w_max < byt_d ) {
            return ur_cue_meme;
          }
          u3i_slab_init(&sab_u, 3, byt_d);
          ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
          *out = u3i_slab_mint_bytes(&sab_u);
        }

        ur_dictn_put(rot_u, dic_u, bit_d, *out);
        return ur_cue_good;
      }

      case U3S_RAM_TAG_BOB: {
        //  read mat(mug) + mat(seq) and produce bob atom
        //
        c3_d mug_d, seq_d;

        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }
        else if ( 62 < len_d ) {
          return ur_cue_meme;
        }
        mug_d = ur_bsr64_any(red_u, len_d);

        if ( ur_cue_good != (res_e = ur_bsr_rub_len(red_u, &len_d)) ) {
          return res_e;
        }
        else if ( 62 < len_d ) {
          return ur_cue_meme;
        }
        seq_d = ur_bsr64_any(red_u, len_d);

        *out = u3i_blob((c3_h)mug_d, (c3_h)seq_d);

        ur_dictn_put(rot_u, dic_u, bit_d, *out);
        return ur_cue_good;
      }
    }
  }
}

/* _cs_tap_xeno(): tap on-loom, with off-loom dictionary.
*/
static u3_weak
_cs_tap_xeno(u3_cue_xeno*  sil_u,
             c3_d          len_d,
             const c3_y*   byt_y)
{
  ur_bsr_t      red_u = {0};
  ur_dictn_t*   dic_u = &sil_u->dic_u;
  u3a_pile      pil_u;
  _tap_frame_t* fam_u;
  ur_cue_res_e  res_e;
  u3_noun         ref;

  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  if ( ur_cue_good != (res_e = ur_bsr_init(&red_u, len_d, byt_y)) ) {
    return u3_none;
  }
  else if ( 0x7ffffffffffffffULL < len_d ) {
    return u3_none;
  }

  res_e = _cs_tap_xeno_next(&pil_u, &red_u, dic_u, &ref);

  if (  (c3n == u3a_pile_done(&pil_u))
     && (ur_cue_good == res_e) )
  {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash result and read the tail
      //
      if ( u3_none == fam_u->ref ) {
        fam_u->ref = ref;
        res_e = _cs_tap_xeno_next(&pil_u, &red_u, dic_u, &ref);
        fam_u = u3a_peek(&pil_u);
      }
      //  tail-frame: build cell and pop stack
      //
      else {
        ur_root_t* rot_u = 0;
        ref   = u3nc(fam_u->ref, ref);
        ur_dictn_put(rot_u, dic_u, fam_u->bit_d, ref);
        fam_u = u3a_pop(&pil_u);
      }
    }
    while (  (c3n == u3a_pile_done(&pil_u))
          && (ur_cue_good == res_e) );
  }

  if ( ur_cue_good == res_e ) {
    return ref;
  }

  //  on failure, unwind and discard intermediates
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
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

/* u3s_tap_xeno(): tap on-loom, with off-loom dictionary.
*/
u3_weak
u3s_tap_xeno(c3_d len_d, const c3_y* byt_y)
{
  //  validate header
  //
  if (  (len_d < 5)
     || (0 != memcmp(byt_y, U3S_RAM_MAGIC, 4))
     || (U3S_RAM_VERSION != byt_y[4]) )
  {
    return u3_none;
  }

  {
    u3_cue_xeno* sil_u = u3s_cue_xeno_init();
    u3_weak        som;

    //  decode after the 5-byte header
    //
    som = _cs_tap_xeno(sil_u, len_d - 5, byt_y + 5);
    ur_dictn_wipe(&sil_u->dic_u);
    u3s_cue_xeno_done(sil_u);
    return som;
  }
}
