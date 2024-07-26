#include <types.h>
#include <imprison.h>
#include <retrieve.h>
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
static void _x_octs_buffer(u3_atom *q_octs, c3_w p_octs_w, c3_y** buf_y,
                           c3_w* len_w, c3_w* lead_w)
{
  *len_w = u3r_met(3, *q_octs);

  if (c3y == u3a_is_cat(*q_octs)) {
    *buf_y = (c3_y*)q_octs;
  }
  else {
    u3a_atom* ptr_a = u3a_to_ptr(*q_octs);
    *buf_y = (c3_y*)ptr_a->buf_w;
  }

  *lead_w = 0;

  if (p_octs_w > *len_w) {
    *lead_w = p_octs_w - *len_w;
  }
  else {
    *len_w = p_octs_w;
  }

}

u3_noun _qe_bytestream_rip_octs(u3_atom p_octs, u3_atom q_octs) {

  c3_w p_octs_w;

  if (c3n == u3r_safe_word(p_octs, &p_octs_w)) {
    return u3m_bail(c3__exit);
  }

  if (p_octs_w == 0) {
    return u3_nul;
  }

  c3_w len_w, lead_w;
  c3_y* buf_y;

  _x_octs_buffer(&q_octs, p_octs_w, &buf_y, &len_w, &lead_w);

  buf_y += len_w;

  u3_noun rip = u3_nul;

  while (lead_w--) {
    rip = u3nc(u3i_word(0x0), rip);
  }

  while (len_w--) {
    rip = u3nc(*(--buf_y), rip);
  }

  return rip;
}
u3_noun
u3we_bytestream_rip_octs(u3_noun cor){

  u3_noun sam = u3x_at(u3x_sam, cor);

  u3_atom p_octs, q_octs;
  _x_octs(sam, &p_octs, &q_octs);

  return _qe_bytestream_rip_octs(p_octs, q_octs);

}

u3_noun
_qe_bytestream_cat_octs(u3_noun octs_a, u3_noun octs_b) {

  u3_atom p_octs_a, p_octs_b;
  u3_atom q_octs_a, q_octs_b;

  _x_octs(octs_a, &p_octs_a, &q_octs_a);
  _x_octs(octs_b, &p_octs_b, &q_octs_b);

  c3_w p_octs_a_w, p_octs_b_w;

  if ( c3n == u3r_safe_word(p_octs_a, &p_octs_a_w) ||
       c3n == u3r_safe_word(p_octs_b, &p_octs_b_w)) {
    u3m_bail(c3__exit);
  }

  if (p_octs_a_w == 0) {
    return octs_b;
  }
  if (p_octs_b_w == 0) {
    return octs_a;
  }

  c3_w  len_w, lem_w;
  c3_w  lead_w, leaf_w;

  c3_y* sea_y;
  c3_y* seb_y;

  _x_octs_buffer(&q_octs_a, p_octs_a_w, &sea_y, &len_w, &lead_w);
  _x_octs_buffer(&q_octs_b, p_octs_b_w, &seb_y, &lem_w, &leaf_w);

  // XX In the far distant future, in a land far away,
  // this might actually lead to integer overflow
  //
  c3_d p_octs_d = p_octs_a_w + lem_w;

  u3i_slab sab_u;

  u3i_slab_bare(&sab_u, 3, p_octs_d);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  memcpy(sab_u.buf_y, sea_y, len_w);
  memset(sab_u.buf_y + len_w, 0, lead_w);
  memcpy(sab_u.buf_y + p_octs_a_w, seb_y, lem_w);

  u3_noun q_octs = u3i_slab_mint(&sab_u);

  return u3nc(u3i_chub(p_octs_d), q_octs);
}

u3_noun
u3we_bytestream_cat_octs(u3_noun cor){

  u3_noun octs_a, octs_b;

  u3x_mean(cor, u3x_sam_2, &octs_a, u3x_sam_3, &octs_b, 0);

  return _qe_bytestream_cat_octs(octs_a, octs_b);

}
