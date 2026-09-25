/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/*
  Get the lowest `n` bits of a word `w` using a bitmask.
*/
#define TAKEBITS(n,w) \
  ((n)==u3a_word_bits) ? (w) :   \
  ((n)==0)  ? 0   :   \
  ((w) & (((c3_w)1 << (n)) - 1))

/*
  Divide, rounding up.
*/
#define DIVCEIL(x,y) \
  (x==0) ? 0 :       \
  1 + ((x - 1) / y);

/*
  `ripn` breaks `atom` into a list of blocks, of bit-width `bits`. The
  resulting list will be least-significant block first.

  XX TODO This only handles cases where the bit-width is <= u3a_word_bits.

  For each block we produce, we need to grab the relevant words inside
  `atom`, so we first compute their indicies.

  `ins_idx` is the word-index of the least-significant word we
  care about, and `sig_idx` is the word after that.

  Next we grab those words (`ins_word` and `sig_word`) from the atom
  using `u3r_word`. word that `sig_idx` might be out-of-bounds for the
  underlying array of `atom`, but `u3r_word` returns 0 in that case,
  which is exatly what we want.

  Now, we need to grab the relevant bits out of both words, and combine
  them. `bits_rem_in_ins_word` is the number of remaining (insignificant)
  bits in `ins_word`, `nbits_ins` is the number of bits we want from the
  less-significant word, and `nbits_sig` from the more-significant one.

  Take the least significant `nbits_sig` bits from `sig_word`, and take
  the slice we care about from `ins_word`. In order to take that slice,
  we drop `bits_rem_in_ins_word` insignificant bits, and then take the
  `nbits_sig` most-significant bits.

  Last, we slice out those bits from the two words, combine them into
  one word, and cons them onto the front of the result.
*/
/* _rip_word(): word [i_w] of a word buffer, zero past its end.
*/
static inline c3_w
_rip_word(const c3_w* buf_w, c3_w len_w, c3_w i_w)
{
  return ( i_w < len_w ) ? buf_w[i_w] : 0;
}

static u3_noun
_bit_rip(u3_atom bits, const c3_w* buf_w, c3_w len_w, c3_w bit_width)
{
  if ( bits==0 || bits>(u3a_word_bits-1)) {
    return u3m_bail(c3__fail);
  }

  c3_w num_blocks = DIVCEIL(bit_width, bits);

  u3_noun res = u3_nul;

  for ( c3_w blk = 0; blk < num_blocks; blk++ ) {
    c3_w next_blk = blk + 1;
    c3_w blks_rem = num_blocks - next_blk;
    c3_w bits_rem = blks_rem * bits;
    c3_w ins_idx  = bits_rem / u3a_word_bits;
    c3_w sig_idx  = ins_idx + 1;

    c3_w bit_rems_in_ins_word = bits_rem % u3a_word_bits;

    c3_w ins_word  = _rip_word(buf_w, len_w, ins_idx);
    c3_w sig_word  = _rip_word(buf_w, len_w, sig_idx);
    c3_w nbits_ins = c3_min(bits, u3a_word_bits - bit_rems_in_ins_word);
    c3_w nbits_sig = bits - nbits_ins;

    c3_w ins_word_bits = TAKEBITS(nbits_ins, ins_word >> bit_rems_in_ins_word);
    c3_w sig_word_bits = TAKEBITS(nbits_sig, sig_word);

    c3_w item = ins_word_bits | (sig_word_bits << nbits_ins);

    res = u3nc(item, res);
  }

  return res;
}

static u3_noun
_block_rip(u3_atom bloq, const c3_w* buf_w, c3_w wor_w, c3_w bit_w)
{

  c3_g bloq_g = bloq;

  /*
    This is a fast-path for the case where all the resulting blocks will
    fit in (u3a_word_bits-1)-bit direct atoms.
  */
  if ( bloq_g < u3a_word_bits_log ) {                                   //  produce direct atoms
    u3_noun acc     = u3_nul;

    c3_w met_w   = (bit_w + ((c3_w)1 << bloq_g) - 1) >> bloq_g;  //  num blocks in atom
    c3_w nbits_w = (c3_w)1 << bloq_g;                   //  block size in bits
    c3_w bmask_w = ((c3_w)1 << nbits_w) - 1;            //  result mask

    for ( c3_w i_w = 0; i_w < met_w; i_w++ ) {          //  `i_w` is block index
      c3_w nex_w = i_w + 1;                             //  next block
      c3_w pat_w = met_w - nex_w;                       //  blks left after this
      c3_w bit_w = pat_w << bloq_g;                     //  bits left after this
      c3_w idx_w = bit_w >> u3a_word_bits_log;                          //  wrds left after this
      c3_w sif_w = bit_w & (u3a_word_bits-1);                          //  bits left in word
      c3_w src_w = _rip_word(buf_w, wor_w, idx_w);      //  find word by index
      c3_w rip_w = (src_w >> sif_w) & bmask_w;          //  get item from word

      acc = u3nc(rip_w, acc);
    }

    return acc;
  }

  u3_noun acc   = u3_nul;
  c3_w    met_w = (bit_w + ((c3_w)1 << bloq_g) - 1) >> bloq_g;
  c3_w    len_w = wor_w;
  c3_g    san_g = (bloq_g - u3a_word_bits_log);
  c3_w    san_w = (c3_w)1 << san_g;
  c3_w    dif_w = (met_w << san_g) - len_w;
  c3_w    tub_w = ((dif_w == 0) ? san_w : (san_w - dif_w));

  for ( c3_w i_w = 0; i_w < met_w; i_w++ ) {
    c3_w     pat_w = (met_w - (i_w + 1));
    c3_w     wut_w = (pat_w << san_g);
    c3_w     sap_w = ((0 == i_w) ? tub_w : san_w);
    c3_w       j_w;
    u3_atom    rip;
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, u3a_word_bits_log, sap_w);

    for ( j_w = 0; j_w < sap_w; j_w++ ) {
      sab_u.buf_w[j_w] = _rip_word(buf_w, wor_w, wut_w + j_w);
    }

    rip = u3i_slab_mint(&sab_u);
    acc = u3nc(rip, acc);
    len_w -= san_w;
  }

  return acc;
}

u3_noun
u3qc_rip(u3_atom a,
         u3_atom b,
         u3_atom c)
{

  if ( c3n == u3a_is_cat(a) ) {
    return u3m_bail(c3__fail);
  }

  if ( c3n == u3a_is_cat(b) ) {
    return u3m_bail(c3__fail);
  }

  if ( a >= u3a_word_bits ) {
    return u3m_bail(c3__fail);
  }

  //  every path reads [c] as a word buffer: a loom atom's own words, a
  //  bob's mapping, or a direct atom's value in the view.  each is
  //  zero-padded to a whole word, so a bob is never materialized and
  //  the chunk loops see the same bytes for every kind.
  //
  u3r_view vue_u;
  u3r_view_init(&vue_u, c);

  const c3_w* buf_w = (const c3_w*)vue_u.byt_y;
  c3_w        wor_w = (vue_u.len_w + u3a_word_bytes - 1) >> u3a_word_bytes_shift;
  c3_w        bit_w = (c3_w)u3r_view_met(&vue_u);
  u3_noun     pro;

  if ( 1 == b ) {
    pro = _block_rip(a, buf_w, wor_w, bit_w);
  }
  else if ( 0 == a ) {
    pro = _bit_rip(b, buf_w, wor_w, bit_w);
  }
  else {
    u3i_slab sab_u;
    c3_w     met_w = (bit_w + ((c3_w)1 << a) - 1) >> a;
    c3_w     len_w = DIVCEIL(met_w, b);
    pro = u3_nul;

    for (c3_w i_w = len_w; 0 < i_w; i_w--) {
      u3i_slab_init(&sab_u, a, b);
      u3r_chop_words(a, (i_w - 1) * b, b, 0, sab_u.buf_w, wor_w, (c3_w*)buf_w);
      pro = u3nc(u3i_slab_mint(&sab_u), pro);
    }
  }

  u3r_view_done(&vue_u);
  return pro;
}

u3_noun
u3wc_rip(u3_noun cor)
{
  u3_atom bloq, step;
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a},
                {u3x_sam_3, &b});
  u3x_bite(a, &bloq, &step);

  return u3qc_rip(bloq, step, u3x_atom(b));
}

u3_noun
u3kc_rip(u3_atom a,
         u3_atom b,
         u3_atom c)
{
  u3_noun pro = u3qc_rip(a, b, c);
  u3z(a); u3z(b); u3z(c);
  return pro;
}
