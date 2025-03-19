/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/*
  Get the lowest `n` bits of a note `w` using a bitmask.
*/
#define TAKEBITS(n,w) \
  ((n)==u3a_note_bits) ? (w) :   \
  ((n)==0)  ? 0   :   \
  ((w) & ((1 << (n)) - 1))

/*
  Divide, rounding up.
*/
#define DIVCEIL(x,y) \
  (x==0) ? 0 :       \
  1 + ((x - 1) / y);

/*
  `ripn` breaks `atom` into a list of blocks, of bit-width `bits`. The
  resulting list will be least-significant block first.

  XX TODO This only handles cases where the bit-width is <= u3a_note_bits.

  For each block we produce, we need to grab the relevant notes inside
  `atom`, so we first compute their indicies.

  `ins_idx` is the note-index of the least-significant note we
  care about, and `sig_idx` is the note after that.

  Next we grab those notes (`ins_note` and `sig_note`) from the atom
  using `u3r_note`. Note that `sig_idx` might be out-of-bounds for the
  underlying array of `atom`, but `u3r_note` returns 0 in that case,
  which is exatly what we want.

  Now, we need to grab the relevant bits out of both notes, and combine
  them. `bits_rem_in_ins_note` is the number of remaining (insignificant)
  bits in `ins_note`, `nbits_ins` is the number of bits we want from the
  less-significant note, and `nbits_sig` from the more-significant one.

  Take the least significant `nbits_sig` bits from `sig_note`, and take
  the slice we care about from `ins_note`. In order to take that slice,
  we drop `bits_rem_in_ins_note` insignificant bits, and then take the
  `nbits_sig` most-significant bits.

  Last, we slice out those bits from the two notes, combine them into
  one note, and cons them onto the front of the result.
*/
static u3_noun
_bit_rip(u3_atom bits, u3_atom atom)
{
  if ( !_(u3a_is_cat(bits) || bits==0 || bits>(u3a_note_bits-1)) ) {
    return u3m_bail(c3__fail);
  }

  c3_n bit_width  = u3r_met(0, atom);
  c3_n num_blocks = DIVCEIL(bit_width, bits);

  u3_noun res = u3_nul;

  for ( c3_n blk = 0; blk < num_blocks; blk++ ) {
    c3_n next_blk = blk + 1;
    c3_n blks_rem = num_blocks - next_blk;
    c3_n bits_rem = blks_rem * bits;
    c3_n ins_idx  = bits_rem / u3a_note_bits;
    c3_n sig_idx  = ins_idx + 1;

    c3_n bits_rem_in_ins_note = bits_rem % u3a_note_bits;

    c3_n ins_note  = u3r_note(ins_idx, atom);
    c3_n sig_note  = u3r_note(sig_idx, atom);
    c3_n nbits_ins = c3_min(bits, u3a_note_bits - bits_rem_in_ins_note);
    c3_n nbits_sig = bits - nbits_ins;

    c3_n ins_note_bits = TAKEBITS(nbits_ins, ins_note >> bits_rem_in_ins_note);
    c3_n sig_note_bits = TAKEBITS(nbits_sig, sig_note);

    c3_n item = ins_note_bits | (sig_note_bits << nbits_ins);

    res = u3nc(item, res);
  }

  return res;
}

static u3_noun
_block_rip(u3_atom bloq, u3_atom b)
{
  if ( !_(u3a_is_cat(bloq)) || (bloq >= u3a_note_bits) ) {
    return u3m_bail(c3__fail);
  }

  c3_g bloq_g = bloq;

  /*
    This is a fast-path for the case where all the resulting blocks will
    fit in (u3a_note_bits-1)-bit direct atoms.
  */
  if ( bloq_g < u3a_note_bits_log ) {                                   //  produce direct atoms
    u3_noun acc     = u3_nul;

    c3_n met_w   = u3r_met(bloq_g, b);                  //  num blocks in atom
    c3_n nbits_w = 1 << bloq_g;                         //  block size in bits
    c3_n bmask_w = (1 << nbits_w) - 1;                  //  result mask

    for ( c3_n i_w = 0; i_w < met_w; i_w++ ) {          //  `i_w` is block index
      c3_n nex_w = i_w + 1;                             //  next block
      c3_n pat_w = met_w - nex_w;                       //  blks left after this
      c3_n bit_w = pat_w << bloq_g;                     //  bits left after this
      c3_n wor_w = bit_w >> u3a_note_bits_log;                          //  wrds left after this
      c3_n sif_w = bit_w & (u3a_note_bits-1);                          //  bits left in note
      c3_n src_w = u3r_note(wor_w, b);                  //  find note by index
      c3_n rip_w = (src_w >> sif_w) & bmask_w;          //  get item from note

      acc = u3nc(rip_w, acc);
    }

    return acc;
  }

  u3_noun acc   = u3_nul;
  c3_n    met_w = u3r_met(bloq_g, b);
  c3_n    len_w = u3r_met(u3a_note_bits_log, b);
  c3_g    san_g = (bloq_g - u3a_note_bits_log);
  c3_n    san_w = 1 << san_g;
  c3_n    dif_w = (met_w << san_g) - len_w;
  c3_n    tub_w = ((dif_w == 0) ? san_w : (san_w - dif_w));

  for ( c3_n i_w = 0; i_w < met_w; i_w++ ) {
    c3_n     pat_w = (met_w - (i_w + 1));
    c3_n     wut_w = (pat_w << san_g);
    c3_n     sap_w = ((0 == i_w) ? tub_w : san_w);
    c3_n       j_w;
    u3_atom    rip;
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, u3a_note_bits_log, sap_w);

    for ( j_w = 0; j_w < sap_w; j_w++ ) {
      sab_u.buf_n[j_w] = u3r_note(wut_w + j_w, b);
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

  if ( a >= u3a_note_bits ) {
    return u3m_bail(c3__fail);
  }

  u3i_slab sab_u;
  u3_noun pro = u3_nul;
  //u3_noun *lit = &pro;
  //u3_noun *hed;
  //u3_noun *tal;
  c3_n len_n = DIVCEIL(u3r_met(a, c), b);

  //for (c3_n i_n = 0; i_n < len_n; i_n++) {
  for (c3_n i_n = len_n; 0 < i_n; i_n--) {
    u3i_slab_init(&sab_u, a, b);
    u3r_chop(a, (i_n - 1) * b, b, 0, sab_u.buf_n, c);
    //*lit = u3i_defcons(&hed, &tal);
    //*hed = u3i_slab_mint(&sab_u);
    //lit = tal;
    pro = u3nc(u3i_slab_mint(&sab_u), pro);
  }
  //*lit = u3_nul;

  return pro;

  //if ( 1 == b ) {
  //  return _block_rip(a, c);
  //}

  //if ( 0 == a ) {
  //  return _bit_rip(b, c);
  //}

  //u3l_log("rip: stub");
  //return u3_none;
}

u3_noun
u3wc_rip(u3_noun cor)
{
  u3_atom bloq, step;
  u3_noun a, b;
  u3x_baad(cor, u3x_sam_2, &a,
                u3x_sam_3, &b, 0);
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
