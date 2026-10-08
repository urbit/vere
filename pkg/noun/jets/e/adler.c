#include <imprison.h>
#include <jets/k.h>
#include <log.h>
#include <nock.h>
#include <retrieve.h>
#include <types.h>
#include <xtract.h>

static void _x_octs(u3_noun octs, u3_atom* p_octs, u3_atom* q_octs) {

  *p_octs = u3h(octs);
  *q_octs = u3t(octs);

  if (c3n == u3a_is_atom(*p_octs) ||
      c3n == u3a_is_atom(*q_octs)) {
    u3m_bail(c3__exit);
  }
}

//  _x_octs_done(): release the view the octs were borrowed through,
//  then return [x], which may have been built from them
//
#define _x_octs_done(vue, x)              \
  do {                                    \
    u3_noun _pro = (x);                   \
    u3r_view_done(&(vue));                \
    return _pro;                          \
  } while ( 0 )

static c3_o _x_octs_buffer(u3_atom* p_octs, u3_atom *q_octs,
                           c3_w* p_octs_w, c3_y** buf_y,
                           c3_w* len_w, c3_w* lead_w,
                           u3r_view* vue_u)
{
  if (c3n == u3r_safe_word(*p_octs, p_octs_w)) {
    return c3n;
  }

  //  a view borrows the bytes wherever the atom lives; the caller
  //  releases it through _x_octs_done.  read-only.
  //
  if ( c3n == u3r_view_open(vue_u, *q_octs, c3n) ) {
    return c3n;
  }
  if ( vue_u->byt_d > c3_w_max ) {
    u3m_bail(c3__fail);
  }
  *len_w = (c3_w)vue_u->byt_d;
  *buf_y = (c3_y*)u3r_view_bytes(vue_u);

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

  u3r_view vue = {0};
  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &buf_y,
                            &len_w, &lead_w, &vue)) {
    _x_octs_done(vue, u3_none);
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

  _x_octs_done(vue, u3i_word(sum2_w << 16 | adler_w));
}


u3_noun 
u3we_adler32(u3_noun cor)
{
  u3_noun octs;

  u3x_mean(cor, {u3x_sam, &octs});

  return _qe_adler32(octs);
}
