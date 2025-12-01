#include <imprison.h>
#include <jets/k.h>
#include <log.h>
#include <nock.h>
#include <retrieve.h>
#include <types.h>
#include <xtract.h>

static void _x_octs(u3_noun octs, u3_atom* p_octs, u3_atom* q_octs) {

  if (c3n == u3r_mean(octs,
             2, p_octs,
             3, q_octs, 0)){
    u3m_bail(c3__exit);
  }

  if (c3n == u3a_is_atom(*p_octs) ||
      c3n == u3a_is_atom(*q_octs)) {
    u3m_bail(c3__exit);
  }
}

static c3_o _x_octs_buffer(u3_atom* p_octs, u3_atom *q_octs,
                           c3_w* p_octs_w, c3_y** buf_y,
                           c3_w* len_w, c3_w* lead_w)
{
  if (c3n == u3r_safe_word(*p_octs, p_octs_w)) {
    return c3n;
  }

  *len_w = u3r_met(3, *q_octs);

  if (c3y == u3a_is_cat(*q_octs)) {
    *buf_y = (c3_y*)q_octs;
  }
  else {
    u3a_atom* ptr_a = u3a_to_ptr(*q_octs);
    *buf_y = (c3_y*)ptr_a->buf_w;
  }

  *lead_w = 0;

  if (*p_octs_w > *len_w) {
    *lead_w = *p_octs_w - *len_w;
  }
  else {
    *len_w = *p_octs_w;
  }

  return c3y;
}

#define BASE 65521
#define NMAX 5552

u3_noun _qe_adler32(u3_noun octs)
{
  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w p_octs_w, len_w, lead_w;
  c3_y *buf_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &buf_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  c3_w adler_w, sum2_w;

  adler_w = 0x1;
  sum2_w = 0x0;

  c3_w pos_w = 0;

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

  u3x_mean(cor, u3x_sam, &octs, 0);

  return _qe_adler32(octs);
}
