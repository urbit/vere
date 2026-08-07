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
static u3_noun
_bit_rip(u3_atom bits, u3_atom atom)
{
  if ( bits==0 || bits>(u3a_word_bits-1)) {
    return u3m_bail(c3__fail);
  }

  c3_w bit_width  = u3r_met(0, atom);
  c3_w num_blocks = DIVCEIL(bit_width, bits);

  u3_noun res = u3_nul;

  for ( c3_w blk = 0; blk < num_blocks; blk++ ) {
    c3_w next_blk = blk + 1;
    c3_w blks_rem = num_blocks - next_blk;
    c3_w bits_rem = blks_rem * bits;
    c3_w ins_idx  = bits_rem / u3a_word_bits;
    c3_w sig_idx  = ins_idx + 1;

    c3_w bit_rems_in_ins_word = bits_rem % u3a_word_bits;

    c3_w ins_word  = u3r_word(ins_idx, atom);
    c3_w sig_word  = u3r_word(sig_idx, atom);
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
_block_rip(u3_atom bloq, u3_atom b)
{

  c3_g bloq_g = bloq;

  /*
    This is a fast-path for the case where all the resulting blocks will
    fit in (u3a_word_bits-1)-bit direct atoms.
  */
  if ( bloq_g < u3a_word_bits_log ) {                                   //  produce direct atoms
    u3_noun acc     = u3_nul;

    c3_w met_w   = u3r_met(bloq_g, b);                  //  num blocks in atom
    c3_w nbits_w = (c3_w)1 << bloq_g;                         //  block size in bits
    c3_w bmask_w = ((c3_w)1 << nbits_w) - 1;                  //  result mask

    for ( c3_w i_w = 0; i_w < met_w; i_w++ ) {          //  `i_w` is block index
      c3_w nex_w = i_w + 1;                             //  next block
      c3_w pat_w = met_w - nex_w;                       //  blks left after this
      c3_w bit_w = pat_w << bloq_g;                     //  bits left after this
      c3_w wor_w = bit_w >> u3a_word_bits_log;                          //  wrds left after this
      c3_w sif_w = bit_w & (u3a_word_bits-1);                          //  bits left in word
      c3_w src_w = u3r_word(wor_w, b);                  //  find word by index
      c3_w rip_w = (src_w >> sif_w) & bmask_w;          //  get item from word

      acc = u3nc(rip_w, acc);
    }

    return acc;
  }

  u3_noun acc   = u3_nul;
  c3_w    met_w = u3r_met(bloq_g, b);
  c3_w    len_w = u3r_met(u3a_word_bits_log, b);
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
      sab_u.buf_w[j_w] = u3r_word(wut_w + j_w, b);
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

  //  correctness: the _bit_rip / _block_rip / u3r_chop paths all read
  //  [c]'s buf_w directly (via u3r_word) or call u3r_blob_load per
  //  chunk — the former returns seq_w (wrong) for bob atoms, the
  //  latter allocates a full-size atom per iteration (O(n*blob)
  //  memory churn).  Materialize the bob once up front: single full
  //  loom allocation, correct reads thereafter.  A fully zero-copy
  //  rip would mmap once and build chunks directly, but that needs a
  //  deeper rewrite of _bit_rip and _block_rip.
  //
  u3_atom mat = u3_none;
  if ( c3y == u3a_is_bob(c) ) {
    mat = u3r_blob_load(c, u3C.dir_c);
    if ( u3_none == mat ) {
      return u3m_bail(c3__fail);
    }
    c = mat;
  }

  u3_noun pro;

  if ( 1 == b ) {
    pro = _block_rip(a, c);
  }
  else if ( 0 == a ) {
    pro = _bit_rip(b, c);
  }
  else {
    u3i_slab sab_u;
    pro = u3_nul;
    c3_w len_w = DIVCEIL(u3r_met(a, c), b);

    for (c3_w i_w = len_w; 0 < i_w; i_w--) {
      u3i_slab_init(&sab_u, a, b);
      u3r_chop(a, (i_w - 1) * b, b, 0, sab_u.buf_w, c);
      pro = u3nc(u3i_slab_mint(&sab_u), pro);
    }
  }

  if ( u3_none != mat ) {
    u3z(mat);
  }
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
