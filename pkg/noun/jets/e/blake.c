/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqe_blake2b(u3_atom wid, u3_atom dat,
             u3_atom wik, u3_atom dak,
             u3_atom out)
  {
    c3_w wid_w;
    if ( !u3r_word_fit(&wid_w, wid) ) {
      // impossible to represent an atom this large
      return u3m_bail(c3__fail);
    }
    else {
      // the hoon adjusts these widths to its liking
      int err;
      c3_y  out_y[64], dak_y[64];
      c3_w  wik_w = c3_min(wik, 64),
            out_w = c3_max(1, c3_min(out, 64));
      c3_y *dat_y = u3r_bytes_alloc(0, wid_w, dat);

      u3r_bytes(0, wik_w, dak_y, dak);
      err = urcrypt_blake2(wid_w, dat_y, wik_w, dak_y, out_w, out_y);
      u3a_free(dat_y);

      if ( 0 == err ) {
        return u3i_bytes(out_w, out_y);
      }
      else {
        return u3_none;
      }
    }
  }

  u3_noun
  u3we_blake2b(u3_noun cor)
  {
    u3_noun msg, key, out, // arguments
            wid, dat,      // destructured msg
            wik, dak;      // destructured key

    if ( c3n == u3r_mean(cor, u3x_sam_2, &msg,
                              u3x_sam_6, &key,
                              u3x_sam_7, &out, 0) ||
                u3r_cell(msg, &wid, &dat) || u3ud(wid) || u3ud(dat) ||
                u3r_cell(key, &wik, &dak) || u3ud(wik) || u3ud(dak) ||
                u3ud(out) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("blake2b", _cqe_blake2b(wid, dat, wik, dak, out));
    }
  }

  static u3_atom
  _cqe_blake3_hash(u3_atom wid, u3_atom dat,
             u3_atom key, u3_atom flags, u3_atom out)
  {
    c3_w wid_w, out_w;
    if ( !u3r_word_fit(&wid_w, wid) || !u3r_word_fit(&out_w, out) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y key_y[32];
      u3r_bytes(0, 32, key_y, key);
      c3_y flags_y = u3r_byte(0, flags);
      c3_y *dat_y = u3r_bytes_alloc(0, wid_w, dat);
      u3i_slab sab_u;
      u3i_slab_bare(&sab_u, 3, out_w);
      c3_y* out_y = sab_u.buf_y;
      urcrypt_blake3_hash(wid_w, dat_y, key_y, flags_y, out, out_y);
      u3a_free(dat_y);
      return u3i_slab_mint(&sab_u);
    }
  }

  u3_noun
  u3we_blake3_hash(u3_noun cor)
  {
    u3_noun out, msg,        // arguments
            wid, dat,        // destructured msg
            sam, key, flags; // context

    if ( c3n == u3r_mean(cor, u3x_sam_2, &out,
                              u3x_sam_3, &msg,
                              u3x_con_sam, &sam, 0) ||
                u3ud(out) ||
                u3r_cell(msg, &wid, &dat) || u3ud(wid) || u3ud(dat) ||
                u3r_cell(sam, &key, &flags) || u3ud(key) || u3ud(flags) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("blake3_hash", _cqe_blake3_hash(wid, dat, key, flags, out));
    }
  }

  static u3_noun
  _cqe_blake3_chunk_output(u3_atom wid, u3_atom dat, u3_atom cv, u3_atom counter, u3_atom flags)
  {
    c3_w wid_w;
    if ( !u3r_word_fit(&wid_w, wid) ) {
      return u3m_bail(c3__fail);
    } else {
      c3_y  cv_y[32], block_y[64], block_len;
      c3_y *dat_y = u3r_bytes_alloc(0, wid_w, dat);
      c3_d counter_d = u3r_chub(0, counter);
      c3_y flags_y = u3r_byte(0, flags);
      u3r_bytes(0, 32, cv_y, cv);
      urcrypt_blake3_chunk_output(wid_w, dat_y, cv_y, block_y, &block_len, &counter_d, &flags_y);
      u3a_free(dat_y);
      return u3i_cell(u3i_bytes(32, cv_y), u3i_qual(u3k(counter), u3i_bytes(64, block_y), block_len, flags_y));
    }
  }

  u3_noun
  u3we_blake3_chunk_output(u3_noun cor)
  {
    u3_noun counter, msg,      // arguments
            wid, dat,          // destructured msg
            key, flags;        // context

    if ( c3n == u3r_mean(cor, u3x_sam_2, &counter,
                              u3x_sam_3, &msg,
                              u3x_con_sam_2, &key,
                              u3x_con_sam_3, &flags, 0) ||
                u3r_cell(msg, &wid, &dat) || u3ud(wid) || u3ud(dat) ||
                u3ud(key) || u3ud(flags))
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("blake3_chunk_output", _cqe_blake3_chunk_output(wid, dat, key, counter, flags));
    }
  }

  static u3_atom
  _cqe_blake3_compress(u3_atom cv, u3_atom counter,
                       u3_atom block, u3_atom block_len, u3_atom flags)
  {
    c3_y cv_y[32], block_y[64], out_y[64];
    u3r_bytes(0, 32, cv_y, cv);
    u3r_bytes(0, 64, block_y, block);
    urcrypt_blake3_compress(cv_y, block_y, block_len, counter, flags, out_y);
    return u3i_bytes(64, out_y);
  }

  u3_noun
  u3we_blake3_compress(u3_noun cor)
  {
    u3_noun output = u3x_at(u3x_sam, cor);
    u3_noun cv, counter, block, block_len, flags;  // destructured output

    if ( u3r_quil(output, &cv, &counter, &block, &block_len, &flags) ||
         u3ud(cv) || u3ud(block) || u3ud(block_len) || u3ud(counter) || u3ud(flags))
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("blake3_compress", _cqe_blake3_compress(cv, counter, block, block_len,  flags));
    }
  }
