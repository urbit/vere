/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_scad(u3_atom pub, u3_atom sek, u3_atom sca)
  {
    c3_y pub_y[32];
    c3_y sek_y[64];
    c3_y sca_y[32];

    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(64, sek_y, sek) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(32, sca_y, sca) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    else {
      urcrypt_ed_add_scalar_public_private(pub_y, sek_y, sca_y);
      return u3nc(u3i_bytes(32, pub_y), u3i_bytes(64, sek_y));
    }
  }

  u3_noun
  u3wee_scad(u3_noun cor)
  {
    u3_noun pub, sek, sca;
    if ( (c3n == u3r_mean(cor,
                         u3x_sam_2, &pub,
                         u3x_sam_6, &sek,
                         u3x_sam_7, &sca, 0)) ||
         (c3n == u3ud(pub)) ||
         (c3n == u3ud(sek)) ||
         (c3n == u3ud(sca)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqee_scad(pub, sek, sca);
    }
  }

  static u3_atom
  _cqee_scas(u3_atom sek, u3_atom sca)
  {
    c3_y sek_y[64];
    c3_y sca_y[32];

    if ( 0 != u3r_bytes_fit(64, sek_y, sek) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(32, sca_y, sca) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    else {
      urcrypt_ed_add_scalar_private(sek_y, sca_y);
      return u3i_bytes(64, sek_y);
    }
  }

  u3_noun
  u3wee_scas(u3_noun cor)
  {
    u3_noun sek, sca;
    if ( (c3n == u3r_mean(cor,
                         u3x_sam_2, &sek,
                         u3x_sam_3, &sca, 0)) ||
         (c3n == u3ud(sek)) ||
         (c3n == u3ud(sca)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqee_scas(sek, sca);
    }
  }

  static u3_atom
  _cqee_scap(u3_atom pub, u3_atom sca)
  {
    c3_y pub_y[32];
    c3_y sca_y[32];

    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(32, sca_y, sca) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    else {
      urcrypt_ed_add_scalar_public(pub_y, sca_y);
      return u3i_bytes(32, pub_y);
    }
  }

  u3_noun
  u3wee_scap(u3_noun cor)
  {
    u3_noun pub, sca;
    if ( (c3n == u3r_mean(cor,
                         u3x_sam_2, &pub,
                         u3x_sam_3, &sca, 0)) ||
         (c3n == u3ud(pub)) ||
         (c3n == u3ud(sca)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqee_scap(pub, sca);
    }
  }


