#include "jets/q.h"
#include "jets/w.h"

#include "allocate.h"
#include "noun.h"


u3_noun u3qe_stream_append_get_bytes(u3_atom n, u3_noun red, u3_noun sea)
{
  u3_atom pos_red;
  u3_atom p_octs_red, q_octs_red;

  u3_atom pos_sea;
  u3_atom p_octs_sea, q_octs_sea;

  u3x_mean(red, 2, &pos_red,
           6, &p_octs_red,
           7, &q_octs_red, 0);

  u3x_mean(sea, 2, &pos_sea,
           6, &p_octs_sea,
           7, &q_octs_sea, 0);

  if (c3y == u3a_is_cat(n) && n == 0) {
    return u3nc(u3k(red), u3k(sea));
  }
  c3_w pos_red_w, pos_sea_w;

  if (c3y != u3r_safe_word(pos_red, &pos_red_w)) {
    return u3_none;
  }

  if (c3y != u3r_safe_word(pos_sea, &pos_sea_w)) {
    return u3_none;
  }

  c3_w n_w;

  if (c3y != u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  
  c3_y *red_buf_y, *sea_buf_y;

  if (c3y == u3a_is_cat(q_octs_red)) {
    red_buf_y = (c3_y*)&q_octs_red;
  }
  else {
    u3a_atom *a = u3a_to_ptr(q_octs_red);
    red_buf_y = (c3_y*)a->buf_w;
  }

  if (c3y == u3a_is_cat(q_octs_sea)) {
    sea_buf_y = (c3_y*)&q_octs_sea;
  }
  else {
    u3a_atom *a = u3a_to_ptr(q_octs_sea);
    sea_buf_y = (c3_y*)a->buf_w;
  }

  c3_w p_octs_red_w, p_octs_sea_w;

  if (c3y != u3r_safe_word(p_octs_red, &p_octs_red_w)) {
    return u3_none;
  }

  if (c3y != u3r_safe_word(p_octs_sea, &p_octs_sea_w)) {
    return u3_none;
  }
  //
  // 1. Verify sea has n bytes from its current position
  // 2. Get current addresses of red and sea
  // 3. Resize red to accomodate n bytes more 
  // 4. Copy data from red to sea
  // 5. Return and update size of red 
  //
  if (pos_sea_w + n_w > p_octs_sea_w) {
    fprintf(stderr, "u3qe_stream_append_get_bytes: sea exhausted\r\n");
    return u3_none;
  }

  // XX is there a way to enlarge an existing storage?
  
  // Make space for expanded red
  //
  c3_i red_len_i = u3r_met(3, q_octs_red);
  c3_i new_red_len_i = p_octs_red_w + n_w;

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, new_red_len_i);

  c3_y* buf_y = sab_u.buf_y;

  // Copy over existing content in red 
  //
  memcpy(buf_y, red_buf_y, red_len_i);

  if (p_octs_red_w > red_len_i) {
    memset(buf_y + red_len_i, 0, p_octs_red_w - red_len_i);
  }

  // Copy n_w bytes from sea
  //
  memcpy(buf_y+p_octs_red_w, sea_buf_y + pos_sea_w, n_w);

  u3_atom new_red = u3nt(u3k(pos_red), 
                         u3i_chub(new_red_len_i),
                         u3i_slab_mint(&sab_u));

  return u3nc(new_red, u3k(sea));
}

u3_noun u3we_stream_append_get_bytes(u3_noun cor){

  u3_atom n;
  u3_noun red, sea;

  u3x_mean(cor, u3x_sam_2, &n,
           u3x_sam_6, &red,
           u3x_sam_7, &sea, 0);

  return u3qe_stream_append_get_bytes(n, red, sea);
}

u3_noun u3qe_stream_append_read_bytes(u3_atom n, u3_noun red, u3_noun sea)
{
  u3_atom pos_red;
  u3_atom p_octs_red, q_octs_red;

  u3_atom pos_sea;
  u3_noun octs_sea;
  u3_atom p_octs_sea, q_octs_sea;

  u3x_mean(red, 2, &pos_red,
           6, &p_octs_red,
           7, &q_octs_red, 0);

  u3x_mean(sea, 2, &pos_sea, 3, &octs_sea, 0);
  u3x_mean(octs_sea, 2, &p_octs_sea, 3, &q_octs_sea, 0);

  if (c3y == u3a_is_cat(n) && n == 0) {
    return u3nc(u3k(red), u3k(sea));
  }
  c3_w pos_red_w, pos_sea_w;

  if (c3y != u3r_safe_word(pos_red, &pos_red_w)) {
    return u3_none;
  }

  if (c3y != u3r_safe_word(pos_sea, &pos_sea_w)) {
    return u3_none;
  }

  c3_w n_w;

  if (c3y != u3r_safe_word(n, &n_w)) {
    return u3_none;
  }
  
  c3_y *red_buf_y, *sea_buf_y;

  if (c3y == u3a_is_cat(q_octs_red)) {
    red_buf_y = (c3_y*)&q_octs_red;
  }
  else {
    u3a_atom *a = u3a_to_ptr(q_octs_red);
    red_buf_y = (c3_y*)a->buf_w;
  }

  if (c3y == u3a_is_cat(q_octs_sea)) {
    sea_buf_y = (c3_y*)&q_octs_sea;
  }
  else {
    u3a_atom *a = u3a_to_ptr(q_octs_sea);
    sea_buf_y = (c3_y*)a->buf_w;
  }

  c3_w p_octs_red_w, p_octs_sea_w;

  if (c3y != u3r_safe_word(p_octs_red, &p_octs_red_w)) {
    return u3_none;
  }

  if (c3y != u3r_safe_word(p_octs_sea, &p_octs_sea_w)) {
    return u3_none;
  }
  //
  // 1. Verify sea has n bytes from its current position
  // 2. Get current addresses of red and sea
  // 3. Resize red to accomodate n bytes more 
  // 4. Copy data from red to sea
  // 5. Return and update size of red 
  //
  if (pos_sea_w + n_w > p_octs_sea_w) {
    fprintf(stderr, "u3qe_stream_append_read_bytes: sea exhausted\r\n");
    return u3_none;
  }

  // XX is there a way to enlarge an existing storage?
  
  // Make space for expanded red
  //
  c3_i red_len_i = u3r_met(3, q_octs_red);
  c3_i new_red_len_i = p_octs_red_w + n_w;

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, new_red_len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  c3_y* buf_y = sab_u.buf_y;

  // Copy over existing content in red 
  //
  memcpy(buf_y, red_buf_y, red_len_i);

  if (p_octs_red_w > red_len_i) {
    memset(buf_y + red_len_i, 0, p_octs_red_w - red_len_i);
  }

  // Copy n_w bytes from sea
  //
  memcpy(buf_y+p_octs_red_w, sea_buf_y + pos_sea_w, n_w);

  u3_atom new_red = u3nt(u3k(pos_red), 
                         u3i_chub(new_red_len_i),
                         u3i_slab_mint(&sab_u));

  return u3nc(new_red, u3nc(u3i_word(pos_sea_w + n_w), u3k(octs_sea)));
}

u3_noun u3we_stream_append_read_bytes(u3_noun cor){

  u3_atom n;
  u3_noun red, sea;

  u3x_mean(cor, u3x_sam_2, &n,
           u3x_sam_6, &red,
           u3x_sam_7, &sea, 0);

  return u3qe_stream_append_read_bytes(n, red, sea);
}

