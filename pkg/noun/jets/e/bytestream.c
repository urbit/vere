#include <types.h>
#include <imprison.h>
#include <jets/k.h>
#include <nock.h>
#include <retrieve.h>
#include <xtract.h>
#include <log.h>

// XX do not crash on indirect atoms, but default to Hoon
// XX use u3i_word to imprison all indirect atoms
//
static void
_x_octs(u3_noun octs, u3_atom* p_octs, u3_atom* q_octs) {

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
static c3_o
_x_octs_buffer(u3_atom* p_octs, u3_atom *q_octs,
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

u3_noun
_qe_bytestream_rip_octs(u3_atom p_octs, u3_atom q_octs) {

  c3_w p_octs_w, len_w, lead_w;
  c3_y* buf_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &buf_y,
                            &len_w, &lead_w)){
    return u3_none;
  }

  if (p_octs_w == 0) {
    return u3_nul;
  }

  u3_noun rip = u3_nul;

  while (lead_w--) {
    rip = u3nc(0x0, rip);
  }

  buf_y += len_w - 1;

  while (len_w--) {
    rip = u3nc(*(buf_y--), rip);
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

  c3_w  p_octs_a_w, p_octs_b_w;
  c3_w  len_w, lem_w;
  c3_w  lead_w, leaf_w;

  c3_y* sea_y;
  c3_y* seb_y;

  if (c3n == _x_octs_buffer(&p_octs_a, &q_octs_a,
                            &p_octs_a_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  if (c3n == _x_octs_buffer(&p_octs_b, &q_octs_b,
                            &p_octs_b_w, &seb_y,
                            &lem_w, &leaf_w)) {
    return u3_none;
  }

  if (p_octs_a_w == 0) {
    return u3k(octs_b);
  }

  if (p_octs_b_w == 0) {
    return u3k(octs_a);
  }

  c3_d p_octs_d = p_octs_a_w + p_octs_b_w;

  u3_noun ret;

  //  Both a and b are 0.
  //
  if (len_w == 0 && lem_w == 0) {
    ret = u3nc(u3i_chub(p_octs_d), u3i_word(0));
  }
  else {
    u3i_slab sab_u;

    u3i_slab_bare(&sab_u, 3, (c3_d)p_octs_a_w + lem_w);
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    memcpy(sab_u.buf_y, sea_y, len_w);
    memset(sab_u.buf_y + len_w, 0, lead_w);
    memcpy(sab_u.buf_y + p_octs_a_w, seb_y, lem_w);

    u3_noun q_octs = u3i_slab_moot(&sab_u);
    ret = u3nc(u3i_chub(p_octs_d), q_octs);
  }
  return ret;
}

u3_noun
u3we_bytestream_cat_octs(u3_noun cor) {

  u3_noun octs_a, octs_b;

  u3x_mean(cor, u3x_sam_2, &octs_a, u3x_sam_3, &octs_b, 0);

  return _qe_bytestream_cat_octs(octs_a, octs_b);

}

u3_noun
_qe_bytestream_can_octs(u3_noun octs_list) {

  if (u3_nul == octs_list) {
    return u3nc(0, 0);
  }

  if (u3_nul == u3t(octs_list)) {
    return u3k(u3h(octs_list));
  }

  /* We can octs in two steps:
   * first loop iteration computes the total required
   * buffer size in bytes, factoring in the leading bytes
   * of the final octs. The second loop iterates over each octs,
   * copying the data to the output buffer.
   */

  // Compute total size
  //
  c3_d tot_d = 0;

  u3_noun octs_list_start = octs_list;
  u3_noun octs = u3_none;
  // Last non-zero octs
  u3_noun last_octs = u3_none;

  while (octs_list != u3_nul) {

    octs = u3h(octs_list);

    if (c3n == u3a_is_atom(u3h(octs)) ||
        c3n == u3a_is_atom(u3t(octs))) {
      u3m_bail(c3__exit);
    }
    c3_w p_octs_w;

    if (c3n == u3r_safe_word(u3h(octs), &p_octs_w)) {
      u3z(octs_list);
      return u3_none;
    }
    // Check for overflow
    //
    if ( p_octs_w > (UINT64_MAX - tot_d)){
      return u3_none;
    }
    tot_d += p_octs_w;

    octs_list = u3t(octs_list);
  }

  //  Compute leading zeros of last non-zero octs -- the buffer
  //  size is decreased by this much.
  //
  //  =leading-zeros (sub p.octs (met 3 q.octs))
  //
  //  p.octs fits into a word -- this has been verified
  //  in the loop above.
  //
  //  The resulting buf_len_w is correct only if the last
  //  octs is non-zero: but at the return u3i_slab_mint
  //  takes care of trimming.
  //
  c3_w last_lead_w = (u3r_word(0, u3h(octs)) - u3r_met(3, u3t(octs)));
  c3_d buf_len_w = tot_d - last_lead_w;

  if (buf_len_w == 0) {
    return u3nc(u3i_word(tot_d), 0);
  }

  u3i_slab sab_u;
  u3i_slab_bare(&sab_u, 3, buf_len_w);
  c3_y* buf_y = sab_u.buf_y;

  sab_u.buf_w[sab_u.len_w - 1] = 0;

  c3_y* sea_y;
  u3_atom p_octs, q_octs;
  c3_w p_octs_w, q_octs_w;
  c3_w len_w, lead_w;

  // Bytes written so far
  //
  c3_d wit_d = 0;

  octs_list = octs_list_start;

  while (octs_list != u3_nul) {

    octs = u3h(octs_list);

    _x_octs(octs, &p_octs, &q_octs);
    if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                              &p_octs_w, &sea_y,
                              &len_w, &lead_w)){
      return u3_none;
    }

    if (p_octs_w == 0) {
      octs_list = u3t(octs_list);
      continue;
    }

    memcpy(buf_y, sea_y, len_w);
    buf_y += len_w;
    wit_d += len_w;

    // More bytes to follow, write leading zeros
    //
    if (wit_d < buf_len_w)  {
      memset(buf_y, 0, lead_w);
      buf_y += lead_w;
      wit_d += lead_w;
    }

    octs_list = u3t(octs_list);
  }

  u3_assert((buf_y - sab_u.buf_y) == buf_len_w);

  return u3nc(u3i_chub(tot_d), u3i_slab_mint(&sab_u));
}

u3_noun
u3we_bytestream_can_octs(u3_noun cor)
{
  u3_noun octs_list;

  u3x_mean(cor, u3x_sam_1, &octs_list, 0);

  return _qe_bytestream_can_octs(octs_list);
}
u3_noun
_qe_bytestream_skip_line(u3_atom pos, u3_noun octs)
{
  c3_w pos_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  while (pos_w < len_w) {
    if (*(sea_y + pos_w) == '\n') {
      break;
    }
    pos_w++;
  }
  // Newline not found, position at the end
  if (*(sea_y + pos_w) != '\n') {
    pos_w = p_octs;
  }
  else {
    pos_w++;
  }

  return u3nc(u3i_word(pos_w), u3k(octs));
}
u3_noun
u3we_bytestream_skip_line(u3_noun cor)
{

  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &pos, u3x_sam_3, &octs, 0);

  return _qe_bytestream_skip_line(pos, octs);

}
u3_noun
_qe_bytestream_find_byte(u3_atom bat, u3_atom pos, u3_noun octs)
{
  c3_w bat_w, pos_w;

  if (c3n == u3r_safe_word(bat, &bat_w) || bat_w > 0xff) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  while (pos_w < len_w) {

    if (*(sea_y + pos_w) == bat_w) {
      return u3nc(u3_nul, u3i_word(pos_w));
    }

    pos_w++;
  }
  //  Here we are sure that:
  //  (1) bat_w has not been found
  //  (2) therefore pos_w == len_w
  //
  //  If bat_w == 0, and there is still input
  //  in the stream, it means pos_w points at
  //  the first leading zero.
  //
  if (pos_w < p_octs && bat_w == 0) {
    return u3nc(u3_nul, u3i_word(pos_w));
  }

  return u3_nul;
}
u3_noun
u3we_bytestream_find_byte(u3_noun cor)
{
  u3_atom bat;
  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &bat,
                u3x_sam_6, &pos,
                u3x_sam_7, &octs, 0);

  return _qe_bytestream_find_byte(bat, pos, octs);
}
u3_noun
_qe_bytestream_seek_byte(u3_atom bat, u3_atom pos, u3_noun octs)
{
  c3_w bat_w, pos_w;

  if (c3n == u3r_safe_word(bat, &bat_w) || bat_w > 0xff) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  while (pos_w < len_w) {

    if (*(sea_y + pos_w) == bat_w) {
      u3_noun idx = u3nc(u3_nul, u3i_word(pos_w));
      u3_noun new_bays = u3nc(u3i_word(pos_w), u3k(octs));
      return u3nc(idx, new_bays);
    }

    pos_w++;
  }

  // find leading zero: see comment in *_find_byte
  //
  if (pos_w < p_octs && bat_w == 0) {
      u3_noun idx = u3nc(u3_nul, u3i_word(pos_w));
      u3_noun new_bays = u3nc(u3i_word(pos_w), u3k(octs));
      return u3nc(idx, new_bays);
  }

  return u3nc(u3_nul, u3nc(u3k(pos), u3k(octs)));

}
u3_noun
u3we_bytestream_seek_byte(u3_noun cor)
{
  u3_atom bat;
  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &bat,
                u3x_sam_6, &pos,
                u3x_sam_7, &octs, 0);

  return _qe_bytestream_seek_byte(bat, pos, octs);
}

u3_noun
_qe_bytestream_read_byte(u3_atom pos, u3_noun octs)
{
  c3_w pos_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  if (pos_w + 1 > p_octs_w) {
    u3m_bail(c3__exit);
  }

  c3_y bat_y;

  if (pos_w < len_w) {
    bat_y = *(sea_y + pos_w);
  }
  else {
    bat_y = 0;
  }

  u3_noun new_bays = u3nc(u3i_word(pos_w + 1), u3k(octs));

  return u3nc(bat_y, new_bays);
}

u3_noun
u3we_bytestream_read_byte(u3_noun cor)
{
  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &pos,
                u3x_sam_3, &octs, 0);

  return _qe_bytestream_read_byte(pos, octs);
}

u3_noun
_qe_bytestream_read_octs(u3_atom n, u3_atom pos, u3_noun octs)
{
  c3_w n_w, pos_w;

  if (c3n == u3r_safe_word(n, &n_w)) {
    return u3_none;
  }

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  if (n_w == 0) {
    return u3nc(u3nc(0,0), u3nc(u3k(pos), u3k(octs)));
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  if (pos_w + n_w > p_octs_w) {
    u3m_bail(c3__exit);
  }

  // Number of bytes to read, excluding leading zeros
  //
  c3_w red_w = n_w;

  if (pos_w + n_w > len_w) {
    if (pos_w < len_w) {
      red_w = len_w - pos_w;
    }
    // leading zeros - nothing to read
    //
    else {
      red_w = 0;
    }
  }

  u3_noun read_octs;

  if (red_w == 0) {
    read_octs = u3nc(u3i_word(n_w), 0);
  }
  else {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 3, n_w);
    sab_u.buf_w[sab_u.len_w - 1]  = 0;

    memcpy(sab_u.buf_y, sea_y + pos_w, red_w);

    if (red_w < n_w) {
      memset(sab_u.buf_y + red_w, 0, (n_w - red_w));
    }

    read_octs = u3nc(u3i_word(n_w), u3i_slab_moot(&sab_u));
  }

  u3_noun new_bays = u3nc(u3i_word(pos_w + n_w), u3k(octs));

  return u3nc(read_octs, new_bays);
}

u3_noun
u3we_bytestream_read_octs(u3_noun cor)
{
  u3_atom n;
  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &n,
                u3x_sam_6, &pos,
                u3x_sam_7, &octs, 0);

  return _qe_bytestream_read_octs(n, pos, octs);
}


u3_noun
_qe_peek_octs(c3_w n_w, c3_w pos_w, c3_w p_octs_w, c3_y* sea_y,
                      c3_w len_w)
{
  if (n_w == 0) {
    return u3nc(0, 0);
  }

  if (pos_w + n_w > p_octs_w) {
    return u3m_bail(c3__exit);
  }

  // Read leading zeros only
  //
  if (pos_w >= len_w) {
    return u3nc(u3i_word(n_w), 0);
  }
  // Number of remaining buffer bytes
  c3_w reb_w = len_w - pos_w;

  u3i_slab sab_u;
  c3_w my_len_w;

  if (n_w < reb_w) {
    my_len_w = n_w;
  }
  else {
    my_len_w = reb_w;
  }
  u3i_slab_bare(&sab_u, 3, my_len_w);
  sab_u.buf_w[sab_u.len_w - 1] = 0;
  memcpy(sab_u.buf_y, sea_y + pos_w, my_len_w);

  return u3nc(u3i_word(n_w), u3i_slab_moot(&sab_u));
}
u3_noun _qe_bytestream_chunk(u3_atom size, u3_noun pos, u3_noun octs)
{
  c3_w size_w, pos_w;

  if (c3n == u3r_safe_word(size, &size_w)) {
    return u3_none;
  }

  if (size_w == 0) {
    return u3_nul;
  }

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  u3_noun hun = u3_nul;

  while (pos_w < p_octs) {
    // Remaining bytes
    //
    c3_w rem = (p_octs - pos_w);

    if (rem < size) {
      u3_noun octs = _qe_peek_octs(rem, pos_w, p_octs_w, sea_y,
                                   len_w);
      hun = u3nc(octs, hun);
      pos_w += rem;
    }
    else {
      u3_noun octs = _qe_peek_octs(size, pos_w, p_octs_w, sea_y,
                                   len_w);
      hun = u3nc(octs, hun);
      pos_w += size;
    }
  }

  return u3kb_flop(hun);
}

u3_noun
u3we_bytestream_chunk(u3_noun cor)
{
  u3_atom size;
  u3_atom pos;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &size,
                u3x_sam_6, &pos,
                u3x_sam_7, &octs, 0);

  return _qe_bytestream_chunk(size, pos, octs);
}

u3_noun
_qe_bytestream_extract(u3_noun sea, u3_noun rac)
{
  u3_atom pos;
  u3_noun octs;

  u3x_mean(sea, 2, &pos, 3, &octs, 0);

  c3_w pos_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  u3_noun dal = u3_nul;

  u3_noun new_sea = u3_none;

  while (pos_w < p_octs_w) {
    new_sea = u3nc(u3i_word(pos_w), u3k(octs));
    u3_noun ext = u3x_good(u3n_slam_on(u3k(rac), new_sea));

    u3_atom sip, ken;
    c3_w sip_w, ken_w;

    u3x_mean(ext, 2, &sip, 3, &ken, 0);

    if (c3n == u3r_safe_word(sip, &sip_w)) {
      // XX is u3z necessary here?
      // does memory get freed on bail?
      //
      u3l_log("bytestream: sip fail");
      u3z(dal);
      u3z(ext);
      return u3_none;
    }

    if (c3n == u3r_safe_word(ken, &ken_w)) {
      u3l_log("bytestream: ken fail");
      u3z(dal);
      u3z(ext);
      return u3_none;
    }

    u3z(ext);

    if (sip_w == 0 && ken_w == 0) {
      break;
    }

    if (pos_w + sip_w > p_octs_w) {
      u3z(dal);
      return u3_none;
    }

    pos_w += sip_w;

    if (ken_w == 0) {
      continue;
    }

    u3_noun octs = _qe_peek_octs(ken_w, pos_w, p_octs_w, sea_y, len_w);
    pos_w += ken_w;
    dal = u3nc(octs, dal);
  }

  new_sea = u3nc(u3i_word(pos_w), u3k(octs));

  return u3nc(u3kb_flop(dal), new_sea);
}
u3_noun
u3we_bytestream_extract(u3_noun cor)
{
  u3_noun sea;
  u3_noun rac;

  u3x_mean(cor, u3x_sam_2, &sea,
                u3x_sam_3, &rac, 0);

  return _qe_bytestream_extract(sea, rac);
}

u3_noun
_qe_bytestream_fuse_extract(u3_noun sea, u3_noun rac)
{
  u3_atom pos;
  u3_noun octs;

  u3x_mean(sea, 2, &pos, 3, &octs, 0);

  c3_w pos_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  u3_noun dal = u3_nul;

  u3_noun new_sea = u3_none;

  while (pos_w < p_octs_w) {
    new_sea = u3nc(u3i_word(pos_w), u3k(octs));
    u3_noun ext = u3x_good(u3n_slam_on(u3k(rac), new_sea));

    u3_atom sip, ken;
    c3_w sip_w, ken_w;

    u3x_mean(ext, 2, &sip, 3, &ken, 0);

    if (c3n == u3r_safe_word(sip, &sip_w)) {
      // XX is u3z necessary here?
      // does memory get freed on bail?
      //
      u3l_log("bytestream: sip fail");
      u3z(dal);
      u3z(ext);
      return u3_none;
    }
    if (c3n == u3r_safe_word(ken, &ken_w)) {
      u3l_log("bytestream: ken fail");
      u3z(dal);
      u3z(ext);
      return u3_none;
    }

    u3z(ext);

    if (sip_w == 0 && ken_w == 0) {
      break;
    }

    if (pos_w + sip_w > p_octs_w) {
      u3z(dal);
      return u3_none;
    }

    pos_w += sip_w;

    if (ken_w == 0) {
      continue;
    }

    u3_noun octs = _qe_peek_octs(ken_w, pos_w, p_octs_w, sea_y, len_w);
    pos_w += ken_w;
    dal = u3nc(octs, dal);
  }

  u3_noun lad = u3kb_flop(dal);
  u3_noun data = _qe_bytestream_can_octs(lad);
  u3z(lad);

  new_sea = u3nc(u3i_word(pos_w), u3k(octs));

  return u3nc(data, new_sea);
}

u3_noun
u3we_bytestream_fuse_extract(u3_noun cor)
{
  u3_noun sea;
  u3_noun rac;

  u3x_mean(cor, u3x_sam_2, &sea,
                u3x_sam_3, &rac, 0);

  return _qe_bytestream_fuse_extract(sea, rac);
}

#define BITS_D (sizeof(c3_d)*8)

u3_noun
_qe_bytestream_need_bits(u3_atom n, u3_noun bits)
{
  u3_atom num, bit;
  u3_noun bays;

  u3x_mean(bits, 2, &num,
                 6, &bit,
                 7, &bays, 0);


  c3_w n_w, num_w;
  c3_d bit_d;

  if (c3n == u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(num, &num_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_chub(bit, &bit_d)) {
    return u3_none;
  }

  if (num_w >= n_w) {
    return u3k(bits);
  }

  // How many bytes to read
  //
  c3_w need_bits_w = n_w - num_w;

  // Requires indirect atom, drop to Hoon
  //
  if (need_bits_w > BITS_D) {
    return u3_none;
  }

  c3_w need_bytes_w = need_bits_w / 8;

  if (need_bits_w % 8) {
    need_bytes_w += 1;
  }

  c3_w pos_w;
  u3_atom pos;
  u3_noun octs;


  u3x_mean(bays, 2, &pos, 3, &octs, 0);

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  u3_atom p_octs, q_octs;

  _x_octs(octs, &p_octs, &q_octs);

  c3_w  p_octs_w;
  c3_w  len_w, lead_w;

  c3_y* sea_y;

  if (c3n == _x_octs_buffer(&p_octs, &q_octs,
                            &p_octs_w, &sea_y,
                            &len_w, &lead_w)) {
    return u3_none;
  }

  if (pos_w + need_bytes_w > p_octs_w) {
    u3m_bail(c3__exit);
  }

  while (need_bytes_w--) {

    if (pos_w < len_w) {
      bit_d += *(sea_y + pos_w) << num_w;
    }
    num_w += 8;
    pos_w++;

    u3_assert(num_w <= BITS_D);
  }

  u3_noun new_bays = u3nc(u3i_word(pos_w), u3k(octs));

  return u3nt(u3i_word(num_w), u3i_chub(bit_d), new_bays);
}
// +$  bits  $+  bits
//           $:  num=@ud
//               bit=@ub
//               =bays
//           ==
u3_noun
u3we_bytestream_need_bits(u3_noun cor)
{
  u3_atom n;
  u3_noun bits;

  u3x_mean(cor, u3x_sam_2, &n,
                u3x_sam_3, &bits, 0);

  return _qe_bytestream_need_bits(n, bits);
}

u3_noun
_qe_bytestream_drop_bits(u3_atom n, u3_noun bits)
{

  u3_atom num, bit;
  u3_noun bays;

  u3x_mean(bits, 2, &num,
                 6, &bit,
                 7, &bays, 0);

  c3_w n_w, num_w;
  c3_d bit_d;

  if (c3n == u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(num, &num_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_chub(bit, &bit_d)) {
    return u3_none;
  }

  if(n_w == 0) {
    return u3k(bits);
  }

  c3_w dop_w = n_w;

  if (dop_w > num_w) {
    dop_w = num_w;
  }

  bit_d >>= dop_w;
  num_w -= dop_w;

  return u3nt(u3i_word(num_w), u3i_chub(bit_d), u3k(bays));
}
u3_noun
u3we_bytestream_drop_bits(u3_noun cor)
{
  u3_atom n;
  u3_noun bits;

  u3x_mean(cor, u3x_sam_2, &n,
                u3x_sam_3, &bits, 0);

  return _qe_bytestream_drop_bits(n, bits);
}

u3_noun
_qe_bytestream_peek_bits(u3_atom n, u3_noun bits)
{

  u3_atom num, bit;
  u3_noun bays;

  u3x_mean(bits, 2, &num,
                 6, &bit,
                 7, &bays, 0);

  c3_w n_w, num_w;
  c3_d bit_d;

  if (c3n == u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(num, &num_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_chub(bit, &bit_d)) {
    return u3_none;
  }

  if (n_w == 0) {
    return u3i_word(0);
  }

  if (n_w > num_w) {
    u3m_bail(c3__exit);
  }

  if (n_w > BITS_D) {
    return u3_none;
  }

  if (n_w == BITS_D) {
    return u3i_chub(bit_d);
  }
  else {
    c3_d mak_d = ((c3_d)1 << n_w) - 1;

    return u3i_chub(bit_d & mak_d);
  }
}
u3_noun
u3we_bytestream_peek_bits(u3_noun cor)
{
  u3_atom n;
  u3_noun bits;

  u3x_mean(cor, u3x_sam_2, &n,
                u3x_sam_3, &bits, 0);

  return _qe_bytestream_peek_bits(n, bits);
}

u3_noun
_qe_bytestream_read_bits(u3_atom n, u3_noun bits)
{

  u3_atom num, bit;
  u3_noun bays;

  u3x_mean(bits, 2, &num,
                 6, &bit,
                 7, &bays, 0);

  c3_w n_w, num_w;
  c3_d bit_d;

  if (c3n == u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(num, &num_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_chub(bit, &bit_d)) {
    return u3_none;
  }

  if (n_w > num_w) {
    u3m_bail(c3__exit);
  }

  if (n_w > BITS_D) {
    return u3_none;
  }

  c3_d bet_d = 0;

  if (n_w == BITS_D) {
    bet_d = bit_d;
  }
  else {
    c3_d mak_d = ((c3_d)1 << n_w) - 1;
    bet_d = bit_d & mak_d;
  }

  bit_d >>= n_w;
  num_w -= n_w;

  u3_noun new_bits = u3nt(u3i_word(num_w), u3i_chub(bit_d), u3k(bays));

  return u3nc(u3i_chub(bet_d), new_bits);
}

u3_noun
u3we_bytestream_read_bits(u3_noun cor)
{
  u3_atom n;
  u3_noun bits;

  u3x_mean(cor, u3x_sam_2, &n,
                u3x_sam_3, &bits, 0);

  return _qe_bytestream_read_bits(n, bits);
}

u3_noun
_qe_bytestream_byte_bits(u3_noun bits)
{

  u3_atom num, bit;
  u3_noun bays;

  u3x_mean(bits, 2, &num,
                 6, &bit,
                 7, &bays, 0);

  c3_w num_w;
  c3_d bit_d;

  if (c3n == u3r_safe_word(num, &num_w)) {
    return u3_none;
  }
  if (c3n == u3r_safe_chub(bit, &bit_d)) {
    return u3_none;
  }

  c3_y rem_y = num_w & 0x7;

  if (rem_y == 0) {
    return u3k(bits);
  }

  u3_noun new_bits = u3nt(u3i_word(num_w - rem_y),
                          u3i_chub(bit_d >> rem_y),
                          u3k(bays));

  return new_bits;
}

u3_noun
u3we_bytestream_byte_bits(u3_noun cor)
{
  u3_noun bits;

  u3x_mean(cor, u3x_sam, &bits, 0);

  return _qe_bytestream_byte_bits(bits);
}
