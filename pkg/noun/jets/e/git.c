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

u3_noun u3qe_git_protocol_stream_pkt_lines_on_band(
  u3_atom band, 
  u3_noun sea){

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
  c3_y *buf_y = c3_malloc(len_w);

  if (buf_y == NULL) {
    u3m_bail(c3__fail);
  }

  c3_w pkt_len_w;
  c3_y pkt_band_y;

  while (pos_w < p_octs_w) {

    /*
     * Parse pkt-line length
     */
    if (pos_w + 4 > p_octs_w) {
      c3_free(buf_y);
      fprintf(stderr, "u3qe_git__stream_pkt_lines_on_band: parsing failure\r\n");
      return u3_none;
    }

    pkt_len_w = (_hex_dit(sea_y[0]) << 12) + 
                (_hex_dit(sea_y[1]) << 8)  + 
                (_hex_dit(sea_y[2]) << 4)  +
                _hex_dit(sea_y[3]);
    // fprintf(stderr, "u3qe_git__stream_pkt_lines_on_band: pos_w = %d, pkt_len_w = %d, sea_y = %.10s\r\n", pos_w, pkt_len_w, sea_y);

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
      c3_free(buf_y);
      u3m_bail(c3__fail);
    }

    if (total_w + pkt_len_w > len_w) {

      // Should we malloc & copy here?
      //
      c3_y *new_y = c3_malloc(len_w + 2*PKT_LINE_MAX);

      if ( new_y == NULL) {
        c3_free(buf_y);
        u3m_bail(c3__fail);
      }

      memcpy(new_y, buf_y, len_w);
      len_w += 2*PKT_LINE_MAX;

      c3_free(buf_y);
      buf_y = new_y;
    }

    /*
     * Parse band
     */
    pkt_band_y = *sea_y;

    sea_y += 1;
    pos_w += 1;
    pkt_len_w -= 1;

    memmove(buf_y + total_w, sea_y, pkt_len_w);

    if (pkt_band_y == band_w) {
      // fprintf(stderr, "u3we_git__stream_pkt_lines_on_band: accumulating %d bytes\r\n", pkt_len_w);
      total_w += pkt_len_w;
    }
    else {
      // buf_y[total_w + pkt_len_w] = 0;
      // fprintf(stderr, "[band %d] %s\r\n", pkt_band_y, buf_y + total_w);
    }

    sea_y += pkt_len_w;
    pos_w += pkt_len_w;
  }

  u3_noun octs_red = u3nc(u3i_word(total_w), u3i_bytes(total_w, buf_y));
  c3_free(buf_y);

  return u3nc(u3nc(u3i_word(0), octs_red), u3k(sea));
}
u3_noun u3we_git_pak_expand_delta_object(u3_noun cor) {

  u3_atom band;
  u3_noun sea;

  u3x_mean(cor, u3x_sam_2, &band,
           u3x_sam_3, &sea, 0);

  return u3qe_git_protocol_stream_pkt_lines_on_band(band, sea);
}
