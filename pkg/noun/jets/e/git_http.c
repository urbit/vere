
#include "c3.h"

#include "jets/q.h"
#include "jets/w.h"

#include "allocate.h"
#include "noun.h"

#define FLUSH_PKT 0
#define DELIM_PKT 1
#define END_PKT   2

#define PKT_LINE_MAX  ((1 << 16) - 1 - 4)

static c3_w _hex_dit(c3_y byt)
{
  // XX parse in the main function
  //
  u3_assert(('0' <= byt && byt <= '9') || ('a' <= byt && byt <= 'f'));

  c3_w dit_w;

  if (byt > '9') {
    dit_w = (byt - 'a') + 10;
  }
  else {
    dit_w = (byt - '0');
  }

  return dit_w;
}
static c3_y _to_hex_dit(c3_w byt)
{
  // XX parse in the main function
  //
  u3_assert(byt < 0xff);

  if (byt < 10) {
    return byt + '0';
  }
  else {
    return (byt - 10) + 'a';
  }
}

u3_noun u3qe_git_http_read_pkt_lines_on_band(
  u3_atom band,
  u3_noun sea) {

  if (c3n == u3a_is_cat(band)) {
    u3m_bail(c3__fail);
  }

  c3_w band_w;

  if (c3y != u3r_safe_word(band, &band_w)){
    u3m_bail(c3__fail);
  }

  u3_atom pos, p_octs, q_octs;
  u3x_mean(sea, 2, &pos, 6, &p_octs, 7, &q_octs, 0);

  c3_w pos_w, p_octs_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    u3m_bail(c3__fail);
  }

  if (c3n == u3r_safe_word(p_octs, &p_octs_w)) {
    u3m_bail(c3__fail);
  }

  if (c3n == u3a_is_atom(q_octs)) {
    u3m_bail(c3__fail);
  }

  c3_y *sea_y;

  if (c3y == u3a_is_cat(q_octs)) {
    sea_y = (c3_y*)&q_octs;
  }
  else {
    u3a_atom *a = u3a_to_ptr(q_octs);
    // Verify octs sanity
    //
    u3_assert(p_octs_w == u3r_met(3, q_octs));
    sea_y = (c3_y*)a->buf_w;
  }

  sea_y += pos_w;

  c3_w len_w = PKT_LINE_MAX;
  c3_w total_w = 0;

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, len_w);

  c3_y* buf_y = sab_u.buf_y;

  c3_w pkt_len_w;
  c3_y pkt_band_y;

  while (pos_w < p_octs_w) {

    /*
     * Parse pkt-line length
     */
    if (pos_w + 4 > p_octs_w) {
      c3_free(buf_y);
      fprintf(stderr, "u3qe_git__read_pkt_lines_on_band: parsing failure\r\n");
      return u3_none;
    }

    pkt_len_w = (_hex_dit(sea_y[0]) << 12) + 
                (_hex_dit(sea_y[1]) << 8)  + 
                (_hex_dit(sea_y[2]) << 4)  +
                _hex_dit(sea_y[3]);
    // fprintf(stderr, "u3qe_git__read_pkt_lines_on_band: pos_w = %d, pkt_len_w = %d, sea_y = %.10s\r\n", pos_w, pkt_len_w, sea_y);

    if (FLUSH_PKT == pkt_len_w) {
      break;
    }

    pos_w += 4;
    sea_y += 4;
    pkt_len_w -= 4;

    u3_assert(pkt_len_w <= PKT_LINE_MAX);

    /*
     * Make space for pkt_len_w bytes in the buffer
     */
    if (pos_w + pkt_len_w > p_octs_w) {
      u3i_slab_free(&sab_u);
      u3m_bail(c3__fail);
    }

    /*
     * Parse band
     */
    pkt_band_y = *sea_y;

    sea_y += 1;
    pos_w += 1;
    pkt_len_w -= 1;


    if (pkt_band_y == band_w) {
      if (total_w + pkt_len_w > len_w) {

        len_w += 4*PKT_LINE_MAX;
        u3i_slab_grow(&sab_u, 3, len_w);
        buf_y = sab_u.buf_y;
      }

      memcpy(buf_y + total_w, sea_y, pkt_len_w);
      total_w += pkt_len_w;
    }
    else {
      // buf_y[total_w + pkt_len_w] = 0;
      // fprintf(stderr, "[band %d] %s\r\n", pkt_band_y, buf_y + total_w);
    }

    sea_y += pkt_len_w;
    pos_w += pkt_len_w;
  }
  fprintf(stderr, "Assembled %d bytes\r\n", total_w);
  u3_noun octs_red = u3nc(u3i_word(total_w), u3i_slab_mint(&sab_u));
  return u3nc(octs_red, u3k(sea));
}
u3_noun u3we_git_http_read_pkt_lines_on_band(u3_noun cor) {

  u3_noun sea;
  u3_atom band;

  u3x_mean(cor, u3x_sam_2, &sea,
           u3x_sam_3, &band, 0);

  return u3qe_git_http_read_pkt_lines_on_band(sea, band);
}

#define CHUNK_SZ ((1 << 13) - 4 - 1)

/* XX handle octs with p < met q and p > met q */
u3_noun u3qe_git_http_write_pkt_lines_on_band(
  u3_noun sea, 
  u3_atom band){

  if (c3n == u3a_is_cat(band)) {
    u3m_bail(c3__fail);
  }

  c3_w band_w;

  if (c3y != u3r_safe_word(band, &band_w)){
    u3m_bail(c3__fail);
  }

  u3_atom pos, p_octs, q_octs;
  u3x_mean(sea, 2, &pos, 6, &p_octs, 7, &q_octs, 0);

  c3_w pos_w, p_octs_w;

  if (c3n == u3r_safe_word(pos, &pos_w)) {
    u3m_bail(c3__fail);
  }

  if (c3n == u3r_safe_word(p_octs, &p_octs_w)) {
    u3m_bail(c3__fail);
  }

  if (c3n == u3a_is_atom(q_octs)) {
    u3m_bail(c3__fail);
  }

  c3_y *sea_y;

  // XX This is broken if p does not match 
  // the length of q
  //
  if (c3y == u3a_is_cat(q_octs)) {
    sea_y = (c3_y*)&q_octs;
  }

  else {
    u3a_atom *a = u3a_to_ptr(q_octs);
    // Verify octs sanity
    //
    u3_assert(p_octs_w == u3r_met(3, q_octs));
    sea_y = (c3_y*)a->buf_w;
  }

  sea_y += pos_w;
  
  // XX handle length properly
  const c3_w len_w = (p_octs_w - pos_w);
  const c3_w num_lines_w = (len_w / CHUNK_SZ) + ((len_w % CHUNK_SZ) ? 1 : 0);
  // 4 bytes for packet length, 1 byte for band
  //
  const c3_w out_len_w = len_w + num_lines_w * 5;

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, out_len_w);

  c3_y* buf_y = sab_u.buf_y;

  c3_w rem_w = len_w;

  while (rem_w) {
    const c3_w copy_len_w = (rem_w < CHUNK_SZ) ? rem_w : CHUNK_SZ;
    const c3_w line_len_w = copy_len_w + 5;

    // Write line legth
    //
    buf_y[0] = _to_hex_dit((line_len_w >> 12) & 0xf);
    buf_y[1] = _to_hex_dit((line_len_w >> 8) & 0xf);
    buf_y[2] = _to_hex_dit((line_len_w >> 4) & 0xf);
    buf_y[3] = _to_hex_dit(line_len_w & 0xf);

    // Write band
    //
    buf_y[4] = 1;

    buf_y += 5;

    memcpy(buf_y, sea_y, copy_len_w);

    buf_y += copy_len_w;
    sea_y += copy_len_w;
    rem_w -= copy_len_w;
  }

  u3_assert(buf_y == sab_u.buf_y + out_len_w);

  return u3nc(u3i_chub(out_len_w), u3i_slab_mint(&sab_u));
}
u3_noun u3we_git_http_write_pkt_lines_on_band(u3_noun cor) {

  u3_noun sea;
  u3_atom band;

  u3x_mean(cor, u3x_sam_2, &band,
           u3x_sam_3, &sea, 0);

  return u3qe_git_http_write_pkt_lines_on_band(band, sea);
}
