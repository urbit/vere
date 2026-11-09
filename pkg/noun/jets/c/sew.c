/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"
#include "nock-compile.h"

u3_weak
u3qc_sew(u3_atom a,
         u3_atom b,
         u3_atom c,
         u3_atom d,
         u3_atom e
        )
{
  c3_w b_w, c_w;
  if (0 == c) return u3k(e);
  if ( !_(u3r_safe_word(b, &b_w)) ||
       !_(u3r_safe_word(c, &c_w)) ) {
    return u3_none;
  }
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }

  c3_g a_g = a;
  c3_w len_e_w = u3r_met(a_g, e);
  u3i_slab sab_u;
  c3_w* src_w;
  c3_w len_src_w;
  if ( _(u3a_is_cat(e)) ) {
    len_src_w = e ? 1 : 0;
    src_w = &e;
  }
  else {
    u3a_atom* src_u = u3a_to_ptr(e);
    len_src_w = src_u->len_w;
    src_w = src_u->buf_w;
  }
  u3i_slab_init(&sab_u, a_g, c3_max(len_e_w, b_w + c_w));
  u3r_chop_words(a_g, 0, b_w, 0, sab_u.buf_w, len_src_w, src_w);
  u3r_chop(a_g, 0, c_w, b_w, sab_u.buf_w, d);
  if (len_e_w > b_w + c_w) {
    u3r_chop_words(a_g,
             b_w + c_w,
             len_e_w - (b_w + c_w),
             b_w + c_w,
             sab_u.buf_w,
             len_src_w,
             src_w);
  }
  return u3i_slab_mint(&sab_u);
}

u3_weak
u3wc_sew(u3_noun cor)
{
  u3_noun a, b, c, d, e;
  a = u3h(u3h(u3t(cor)));
  b = u3h(u3h(u3t(u3h(u3t(cor)))));
  c = u3h(u3t(u3h(u3t(u3h(u3t(cor))))));
  d = u3t(u3t(u3h(u3t(u3h(u3t(cor))))));
  e = u3t(u3t(u3h(u3t(cor))));

  if ( (c3n == u3ud(a)) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(c)) ||
       (c3n == u3ud(d)) ||
       (c3n == u3ud(e)) )
  {
    return u3m_bail(c3__fail);
  } else {
    return u3qc_sew(a, b, c, d, e);
  }
}

/* _sew_copy(): u3qc_sew(), which punts on a span that doesn't fit a word,
**              as a total driver: the nock would fail to allocate.
*/
static u3_noun
_sew_copy(u3_atom a, u3_atom b, u3_atom c, u3_atom d, u3_atom e)
{
  u3_weak pro = u3qc_sew(a, b, c, d, e);

  if ( u3_none == pro ) {
    return u3m_bail(c3__fail);
  }
  return pro;
}

/* u3uc_sew(): array driver of u3wc_sew: [a [b c d] e].
*/
u3_noun
u3uc_sew(u3_noun* arg)
{
  if (  (c3n == u3ud(arg[0]))
     || (c3n == u3ud(arg[1]))
     || (c3n == u3ud(arg[2]))
     || (c3n == u3ud(arg[3]))
     || (c3n == u3ud(arg[4])) )
  {
    return u3m_bail(c3__exit);
  }
  return _sew_copy(arg[0], arg[1], arg[2], arg[3], arg[4]);
}

/* _sew_place(): write the low c bloqs of d at bloq b of e in place, if e
**               is an indirect atom nothing else refers to (u3a_is_mutable())
**               and the result fits in its words.  a < 32, c > 0.
**               TRANSFERS e, and produces the result, which may be e
**               itself, shortened, or a direct atom if e came to one; or
**               produces u3_none and leaves e as it was.
*/
static u3_weak
_sew_place(c3_g a_g, c3_w b_w, c3_w c_w, u3_atom d, u3_atom e)
{
  u3a_atom* vat_u;
  c3_w*     buf_w;
  c3_w      len_w;
  c3_d      lo_d, hi_d, wor_d;

  if ( c3n == u3a_is_mutable(u3R, e) ) {
    return u3_none;
  }

  //  the bits written, [lo, hi), and the words the result needs: those
  //  of e, or those up to hi if it reaches past e
  //
  lo_d = (c3_d)b_w << a_g;
  hi_d = ((c3_d)b_w + c_w) << a_g;

  if ( (hi_d >> a_g) != ((c3_d)b_w + c_w) ) {
    return u3_none;
  }

  vat_u = u3a_to_ptr(e);
  buf_w = vat_u->buf_w;
  len_w = vat_u->len_w;
  wor_d = (hi_d + (u3a_word_bits - 1)) >> u3a_word_bits_log;

  if ( wor_d > len_w ) {
    return u3_none;
  }

  //  clear [lo, hi), then xor the bloqs of d in
  //
  {
    c3_w low_w = lo_d >> u3a_word_bits_log;
    c3_w hig_w = hi_d >> u3a_word_bits_log;
    c3_g lob_g = lo_d & (u3a_word_bits - 1);
    c3_g hib_g = hi_d & (u3a_word_bits - 1);

    if ( low_w == hig_w ) {
      buf_w[low_w] &= ~(((((c3_w)1) << (hib_g - lob_g)) - 1) << lob_g);
    }
    else {
      if ( lob_g ) {
        buf_w[low_w++] &= (((c3_w)1) << lob_g) - 1;
      }
      memset(buf_w + low_w, 0, (size_t)(hig_w - low_w) * u3a_word_bytes);
      if ( hib_g ) {
        buf_w[hig_w] &= ~((((c3_w)1) << hib_g) - 1);
      }
    }

    u3r_chop(a_g, 0, c_w, b_w, buf_w, d);
  }

  vat_u->mug_w = 0;

  //  normalize: the write may have cleared the top word
  //
  {
    c3_w new_w = len_w;

    while ( new_w && !buf_w[new_w - 1] ) {
      new_w--;
    }

    if ( 0 == new_w ) {
      u3a_wfree(vat_u);
      return (u3_atom)0;
    }
    else if ( (1 == new_w) && (c3y == u3a_is_cat(buf_w[0])) ) {
      c3_w dat_w = buf_w[0];
      u3a_wfree(vat_u);
      return (u3_atom)dat_w;
    }

    vat_u->len_w = new_w;
  }

  return e;
}

/* u3uc_sew_own(): transferring array driver of u3wc_sew: TRANSFERS the
**                 arguments flagged in own_y, RETAINS the rest.  Edits e
**                 in place if it owns it and nothing else refers to it.
*/
u3_noun
u3uc_sew_own(u3_noun* arg, const c3_y* own_y)
{
  u3_atom a = arg[0], b = arg[1], c = arg[2], d = arg[3], e = arg[4];
  u3_noun pro;
  c3_w    b_w, c_w;

  if (  (c3n == u3ud(a))
     || (c3n == u3ud(b))
     || (c3n == u3ud(c))
     || (c3n == u3ud(d))
     || (c3n == u3ud(e)) )
  {
    return u3m_bail(c3__exit);
  }

  if ( 0 == c ) {
    pro = own_y[4] ? e : u3k(e);
  }
  else if (  own_y[4]
          && (c3y == u3a_is_cat(a)) && (a < u3a_word_bits)
          && (c3y == u3r_safe_word(b, &b_w))
          && (c3y == u3r_safe_word(c, &c_w))
          && (u3_none != (pro = _sew_place(a, b_w, c_w, d, e))) )
  {
    u3nc_stat(mut_d);
  }
  else {
    pro = _sew_copy(a, b, c, d, e);
    if ( own_y[4] ) {
      u3z(e);
    }
  }

  if ( own_y[0] ) { u3z(a); }
  if ( own_y[1] ) { u3z(b); }
  if ( own_y[2] ) { u3z(c); }
  if ( own_y[3] ) { u3z(d); }

  return pro;
}
