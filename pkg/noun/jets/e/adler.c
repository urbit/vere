#include <imprison.h>
#include <jets/k.h>
#include <log.h>
#include <nock.h>
#include <retrieve.h>
#include <types.h>
#include <xtract.h>

static void _x_octs(u3_noun octs, u3_atom* p_octs, u3_atom* q_octs) {

  if (c3n == u3r_mean(octs,
             {2, p_octs},
             {3, q_octs})){
    u3m_bail(c3__exit);
  }

  if (c3n == u3a_is_atom(*p_octs) ||
      c3n == u3a_is_atom(*q_octs)) {
    u3m_bail(c3__exit);
  }
}

#define BASE 65521
#define NMAX 5552

u3_noun _qe_adler32(u3_noun octs)
{
  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w p_octs_w;
  if (c3n == u3r_safe_word(p_octs, &p_octs_w)) {
    return u3_none;
  }

  //  zero-copy view of the atom's significant bytes (mmap for bob).
  //  NB: the legacy direct-pointer path through ptr_a->buf_w read
  //  seq_w for bob atoms, which silently produced wrong checksums;
  //  using u3r_view fixes that bug in addition to avoiding the
  //  full-blob materialization.
  //
  u3r_view vue_u;
  u3r_view_init(&vue_u, q_octs);
  const c3_y* buf_y = vue_u.byt_y;
  c3_w        len_w = vue_u.len_w;

  //  clamp the bytes we'll actually scan to the declared width; the
  //  remainder is "leading zeros" and handled below.
  //
  if (p_octs_w < len_w) {
    len_w = p_octs_w;
  }

  c3_w adler_w = 0x1;
  c3_w sum2_w  = 0x0;
  c3_w pos_w   = 0;

  // Process all non-zero bytes
  //
  while (pos_w < len_w) {

    c3_w rem_w = (len_w - pos_w);

    if (rem_w > NMAX) {
      rem_w = NMAX;
    }

    while (rem_w--) {
      adler_w += *(buf_y + pos_w++);
      sum2_w += adler_w;
    }

    adler_w %= BASE;
    sum2_w %= BASE;
  }

  u3r_view_done(&vue_u);

  // Process leading zeros
  //
  while (pos_w < p_octs_w) {

    c3_w rem_w = (p_octs_w - pos_w);

    if (rem_w > NMAX) {
      rem_w = NMAX;
    }

    // leading zeros: adler sum is unchanged
    sum2_w += rem_w*adler_w;
    pos_w += rem_w;

    adler_w %= BASE;
    sum2_w %= BASE;
  }

  return u3i_word(sum2_w << 16 | adler_w);
}


u3_noun 
u3we_adler32(u3_noun cor)
{
  u3_noun octs;

  u3x_mean(cor, {u3x_sam, &octs});

  return _qe_adler32(octs);
}
