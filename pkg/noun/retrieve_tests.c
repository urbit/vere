/// @file

#include "noun.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

/* _test_mug(): spot check u3r_mug hashes.
*/
static c3_i
_test_mug(void)
{
  c3_i ret_i = 1;

  if ( 0x4d441035 != u3r_mug_c("Hello, world!") ) {
    fprintf(stderr, "fail (a)\r\n");
    ret_i = 0;
  }

  {
    u3_noun a = u3i_string("Hello, world!");

    if ( 0x4d441035 != u3r_mug(a) ) {
      fprintf(stderr, "fail (b)\r\n");
      ret_i = 0;
    }

    u3z(a);
  }

  {
    c3_y byt_y[1];

    if ( 0x79ff04e8 != u3r_mug_bytes(0, 0) ) {
      fprintf(stderr, "fail (c) (0)\r\n");
      ret_i = 0;
    }

    byt_y[0] = 1;

    if ( 0x715c2a60 != u3r_mug_bytes(byt_y, 1) ) {
      fprintf(stderr, "fail (c) (1)\r\n");
      ret_i = 0;
    }

    byt_y[0] = 2;

    if ( 0x718b9468 != u3r_mug_bytes(byt_y, 1) ) {
      fprintf(stderr, "fail (c) (2)\r\n");
      ret_i = 0;
    }
  }

  if ( 0x3a811aec != u3r_mug_both(0x715c2a60, u3r_mug_cell(2, 3)) ) {
    fprintf(stderr, "fail (d)\r\n");
    ret_i = 0;
  }


  {
    if ( 0x192f5588 != u3r_mug_cell(0, 0) ) {
      fprintf(stderr, "fail (e) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x6b32ec46 != u3r_mug_cell(1, 1) ) {
      fprintf(stderr, "fail (e) (2)\r\n");
      ret_i = 0;
    }

    if ( 0x2effe10 != u3r_mug_cell(2, 2) ) {
      fprintf(stderr, "fail (e) (3)\r\n");
      ret_i = 0;
    }
  }

  {
    u3_noun a = u3i_string("xxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    if ( 0x64dfda5c != u3r_mug(a) ) {
      fprintf(stderr, "fail (f)\r\n");
      ret_i = 0;
    }

    u3z(a);
  }

  {
    u3_noun a = u3qc_bex(32);

    if ( 0x7cefb7f != u3r_mug_cell(0, a) ) {
      fprintf(stderr, "fail (g)\r\n");
      ret_i = 0;
    }

    u3z(a);
  }

  {
    u3_noun a = u3ka_dec(u3qc_bex(128));

    if ( 0x2aa06bfc != u3r_mug_cell(a, 1) ) {
      fprintf(stderr, "fail (h)\r\n");
      ret_i = 0;
    }

    u3z(a);
  }

  {
    //  stick some zero bytes in a string
    //
    u3_noun str = u3kc_lsh(3, 1,
                           u3kc_mix(u3qc_bex(212),
                           u3i_string("abcdefjhijklmnopqrstuvwxyz")));

    c3_w  byt_w = u3r_met(3, str);
    c3_w  wor_w = u3r_met(5, str);
    c3_y* str_y = c3_malloc(byt_w);
    c3_w* str_w = c3_malloc(4 * wor_w);
    c3_d  str_d = 0;

    u3r_bytes(0, byt_w, str_y, str);
    u3r_words(0, wor_w, str_w, str);

    str_d |= str_w[0];
    str_d |= ((c3_d)str_w[1] << 32ULL);

    if ( 0x34d08717 != u3r_mug(str) ) {
      fprintf(stderr, "fail (i) (1) \r\n");
      ret_i = 0;
    }
    if ( 0x34d08717 != u3r_mug_bytes(str_y, byt_w) ) {
      fprintf(stderr, "fail (i) (2)\r\n");
      ret_i = 0;
    }
    if ( 0x34d08717 != u3r_mug_words(str_w, wor_w) ) {
      fprintf(stderr, "fail (i) (3)\r\n");
      ret_i = 0;
    }
    if ( u3r_mug_words(str_w, 2) != u3r_mug_chub(str_d) ) {
      fprintf(stderr, "fail (i) (4)\r\n");
      ret_i = 0;
    }

    c3_free(str_y);
    c3_free(str_w);
    u3z(str);
  }

  {
    c3_w  som_w[4] = { 0, 0, 0, 1 };
    u3_noun som    = u3i_words(4, som_w);

    if ( 0x519bd45c != u3r_mug(som) ) {
      fprintf(stderr, "fail (j) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x519bd45c != u3r_mug_words(som_w, 4) ) {
      fprintf(stderr, "fail (j) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w  som_w[4] = { 0, 1, 0, 1 };
    u3_noun som    = u3i_words(4, som_w);

    if ( 0x540eb8a9 != u3r_mug(som) ) {
      fprintf(stderr, "fail (k) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x540eb8a9 != u3r_mug_words(som_w, 4) ) {
      fprintf(stderr, "fail (k) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w  som_w[4] = { 1, 1, 0, 1 };
    u3_noun som    = u3i_words(4, som_w);

    if ( 0x319d28f9 != u3r_mug(som) ) {
      fprintf(stderr, "fail (l) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x319d28f9 != u3r_mug_words(som_w, 4) ) {
      fprintf(stderr, "fail (l) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w  som_w[4] = { 0, 0, 0, 0xffff };
    u3_noun som    = u3i_words(4, som_w);

    if ( 0x5230a260 != u3r_mug(som) ) {
      fprintf(stderr, "fail (m) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x5230a260 != u3r_mug_words(som_w, 4) ) {
      fprintf(stderr, "fail (m) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  return ret_i;
}

/* _dirty(): fill and free atoms of (len_w) words, dirtying the allocator.
**
**  Reads past the end of an atom return zeros only if the memory beyond
**  it happens to be zero, which it is on a freshly mapped page. Dirtying
**  the relevant size classes keeps a broken implementation from passing
**  by luck.
*/
static void
_dirty(c3_w len_w)
{
  u3_noun som[64];
  c3_w*   buf_w = c3_malloc(len_w << 2);
  c3_w    i_w;

  memset(buf_w, 0xff, len_w << 2);

  for ( i_w = 0; i_w < 64; i_w++ ) {
    som[i_w] = u3i_words(len_w, buf_w);
  }

  for ( i_w = 0; i_w < 64; i_w++ ) {
    u3z(som[i_w]);
  }

  c3_free(buf_w);
}

/* _dirty_all(): dirty the size classes the retrieval tests allocate from.
*/
static void
_dirty_all(void)
{
  c3_w len_w;

  for ( len_w = 1; len_w <= 8; len_w++ ) {
    _dirty(len_w);
  }
}

/* _check(): compare (hav_y) against (wan_y), reporting a mismatch.
*/
static c3_i
_check(const c3_c* nam_c,
       const c3_y* hav_y,
       const c3_y* wan_y,
       c3_w        len_w)
{
  c3_w i_w;

  if ( 0 == memcmp(hav_y, wan_y, len_w) ) {
    return 1;
  }

  fprintf(stderr, "fail %s\r\n  have:", nam_c);
  for ( i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, " %02x", hav_y[i_w]);
  }
  fprintf(stderr, "\r\n  want:");
  for ( i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, " %02x", wan_y[i_w]);
  }
  fprintf(stderr, "\r\n");

  return 0;
}

//  test atoms: a direct atom of four significant bytes, and an indirect
//  atom of three words (an odd number, so a chub read is half-populated)
//
#define TEST_CAT  0x45464748
static c3_w _test_dog_w[3] = { 0x03020100, 0x07060504, 0x0b0a0908 };

/* _test_bytes(): u3r_bytes() copy and zero-fill.
*/
static c3_i
_test_bytes(void)
{
  c3_i    ret_i = 1;
  u3_atom cat, dog;
  c3_y    out_y[24];

  _dirty_all();

  cat = TEST_CAT;
  dog = u3i_words(3, _test_dog_w);

  //  direct atom, offset 0, exactly the available length
  //
  {
    c3_y wan_y[4] = { 0x48, 0x47, 0x46, 0x45 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 4, out_y, cat);

    if ( !_check("(a) cat exact", out_y, wan_y, 4) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset 0, more than available
  //
  {
    c3_y wan_y[8] = { 0x48, 0x47, 0x46, 0x45, 0, 0, 0, 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 8, out_y, cat);

    if ( !_check("(b) cat overlong", out_y, wan_y, 8) ) {
      ret_i = 0;
    }
  }

  //  direct atom, nonzero offset straddling the end
  //
  {
    c3_y wan_y[4] = { 0x46, 0x45, 0, 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(2, 4, out_y, cat);

    if ( !_check("(c) cat straddle", out_y, wan_y, 4) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset entirely past the end
  //
  {
    c3_y wan_y[8] = { 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(4, 8, out_y, cat);

    if ( !_check("(d) cat past (1)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(100, 8, out_y, cat);

    if ( !_check("(d) cat past (2)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }
  }

  //  direct atom, zero length
  //
  {
    c3_y wan_y[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 0, out_y, cat);

    if ( !_check("(e) cat empty (1)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(9, 0, out_y, cat);

    if ( !_check("(e) cat empty (2)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset 0, exactly the available length
  //
  {
    c3_y wan_y[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 12, out_y, dog);

    if ( !_check("(f) dog exact", out_y, wan_y, 12) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset 0, more than available
  //
  {
    c3_y wan_y[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 16, out_y, dog);

    if ( !_check("(g) dog overlong", out_y, wan_y, 16) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, nonzero offset straddling the end
  //
  {
    c3_y wan_y[6] = { 10, 11, 0, 0, 0, 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(10, 6, out_y, dog);

    if ( !_check("(h) dog straddle", out_y, wan_y, 6) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset entirely past the end
  //
  {
    c3_y wan_y[8] = { 0 };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(12, 8, out_y, dog);

    if ( !_check("(i) dog past (1)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(1000, 8, out_y, dog);

    if ( !_check("(i) dog past (2)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, zero length
  //
  {
    c3_y wan_y[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(0, 0, out_y, dog);

    if ( !_check("(j) dog empty (1)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }

    memset(out_y, 0xff, sizeof(out_y));
    u3r_bytes(20, 0, out_y, dog);

    if ( !_check("(j) dog empty (2)", out_y, wan_y, 8) ) {
      ret_i = 0;
    }
  }

  u3z(dog);

  return ret_i;
}

/* _test_words(): u3r_words() copy and zero-fill.
*/
static c3_i
_test_words(void)
{
  c3_i    ret_i = 1;
  u3_atom cat, dog;
  c3_w    out_w[8];

  _dirty_all();

  cat = TEST_CAT;
  dog = u3i_words(3, _test_dog_w);

  //  direct atom, offset 0, exactly the available length
  //
  {
    c3_w wan_w[1] = { TEST_CAT };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 1, out_w, cat);

    if ( !_check("(a) cat exact", (c3_y*)out_w, (c3_y*)wan_w, 4) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset 0, more than available
  //
  {
    c3_w wan_w[3] = { TEST_CAT, 0, 0 };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 3, out_w, cat);

    if ( !_check("(b) cat overlong", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset entirely past the end
  //
  {
    c3_w wan_w[3] = { 0, 0, 0 };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(1, 3, out_w, cat);

    if ( !_check("(c) cat past (1)", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(50, 3, out_w, cat);

    if ( !_check("(c) cat past (2)", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }
  }

  //  direct atom, zero length
  //
  {
    c3_w wan_w[2];

    memset(wan_w, 0xff, sizeof(wan_w));
    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 0, out_w, cat);

    if ( !_check("(d) cat empty (1)", (c3_y*)out_w, (c3_y*)wan_w, 8) ) {
      ret_i = 0;
    }

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(7, 0, out_w, cat);

    if ( !_check("(d) cat empty (2)", (c3_y*)out_w, (c3_y*)wan_w, 8) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset 0, exactly the available length
  //
  {
    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 3, out_w, dog);

    if ( !_check("(e) dog exact", (c3_y*)out_w, (c3_y*)_test_dog_w, 12) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset 0, more than available
  //
  {
    c3_w wan_w[5] = { 0x03020100, 0x07060504, 0x0b0a0908, 0, 0 };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 5, out_w, dog);

    if ( !_check("(f) dog overlong", (c3_y*)out_w, (c3_y*)wan_w, 20) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, nonzero offset straddling the end
  //
  {
    c3_w wan_w[3] = { 0x0b0a0908, 0, 0 };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(2, 3, out_w, dog);

    if ( !_check("(g) dog straddle", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset entirely past the end
  //
  {
    c3_w wan_w[3] = { 0, 0, 0 };

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(3, 3, out_w, dog);

    if ( !_check("(h) dog past (1)", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(500, 3, out_w, dog);

    if ( !_check("(h) dog past (2)", (c3_y*)out_w, (c3_y*)wan_w, 12) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, zero length
  //
  {
    c3_w wan_w[2];

    memset(wan_w, 0xff, sizeof(wan_w));
    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(0, 0, out_w, dog);

    if ( !_check("(i) dog empty (1)", (c3_y*)out_w, (c3_y*)wan_w, 8) ) {
      ret_i = 0;
    }

    memset(out_w, 0xff, sizeof(out_w));
    u3r_words(9, 0, out_w, dog);

    if ( !_check("(i) dog empty (2)", (c3_y*)out_w, (c3_y*)wan_w, 8) ) {
      ret_i = 0;
    }
  }

  u3z(dog);

  return ret_i;
}

/* _test_chubs(): u3r_chubs() copy and zero-fill.
*/
static c3_i
_test_chubs(void)
{
  c3_i    ret_i = 1;
  u3_atom cat, dog;
  c3_d    out_d[8];

  _dirty_all();

  cat = TEST_CAT;
  dog = u3i_words(3, _test_dog_w);

  //  direct atom, offset 0, exactly one chub (zero-extended)
  //
  {
    c3_d wan_d[1] = { TEST_CAT };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 1, out_d, cat);

    if ( !_check("(a) cat exact", (c3_y*)out_d, (c3_y*)wan_d, 8) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset 0, more than available
  //
  {
    c3_d wan_d[3] = { TEST_CAT, 0, 0 };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 3, out_d, cat);

    if ( !_check("(b) cat overlong", (c3_y*)out_d, (c3_y*)wan_d, 24) ) {
      ret_i = 0;
    }
  }

  //  direct atom, offset entirely past the end
  //
  {
    c3_d wan_d[2] = { 0, 0 };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(1, 2, out_d, cat);

    if ( !_check("(c) cat past (1)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(25, 2, out_d, cat);

    if ( !_check("(c) cat past (2)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  //  direct atom, zero length
  //
  {
    c3_d wan_d[2];

    memset(wan_d, 0xff, sizeof(wan_d));
    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 0, out_d, cat);

    if ( !_check("(d) cat empty (1)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(4, 0, out_d, cat);

    if ( !_check("(d) cat empty (2)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  //  indirect atom of an odd number of words: the final chub is
  //  half-populated and must zero-extend
  //
  {
    c3_d wan_d[2] = { 0x0706050403020100ULL, 0x000000000b0a0908ULL };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 2, out_d, dog);

    if ( !_check("(e) dog exact", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset 0, more than available
  //
  {
    c3_d wan_d[4] = { 0x0706050403020100ULL, 0x000000000b0a0908ULL, 0, 0 };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 4, out_d, dog);

    if ( !_check("(f) dog overlong", (c3_y*)out_d, (c3_y*)wan_d, 32) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, nonzero offset straddling the end
  //
  {
    c3_d wan_d[2] = { 0x000000000b0a0908ULL, 0 };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(1, 2, out_d, dog);

    if ( !_check("(g) dog straddle", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, offset entirely past the end
  //
  {
    c3_d wan_d[2] = { 0, 0 };

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(2, 2, out_d, dog);

    if ( !_check("(h) dog past (1)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(250, 2, out_d, dog);

    if ( !_check("(h) dog past (2)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  //  indirect atom, zero length
  //
  {
    c3_d wan_d[2];

    memset(wan_d, 0xff, sizeof(wan_d));
    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(0, 0, out_d, dog);

    if ( !_check("(i) dog empty (1)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }

    memset(out_d, 0xff, sizeof(out_d));
    u3r_chubs(5, 0, out_d, dog);

    if ( !_check("(i) dog empty (2)", (c3_y*)out_d, (c3_y*)wan_d, 16) ) {
      ret_i = 0;
    }
  }

  u3z(dog);

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_mug() ) {
    fprintf(stderr, "test_mug: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_mug: ok\n");

  if ( !_test_bytes() ) {
    fprintf(stderr, "test_bytes: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_bytes: ok\n");

  if ( !_test_words() ) {
    fprintf(stderr, "test_words: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_words: ok\n");

  if ( !_test_chubs() ) {
    fprintf(stderr, "test_chubs: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_chubs: ok\n");

  return 0;
}
