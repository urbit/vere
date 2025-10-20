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

    c3_n  byt_w = u3r_met(3, str);
    c3_n  wor_w = u3r_met(5, str);
    c3_y* str_y = c3_malloc(byt_w);
    c3_w_new* str_w = c3_malloc(sizeof(c3_w_new) * wor_w);
    c3_d  str_d = 0;

    u3r_bytes(0, byt_w, str_y, str);
    u3r_words_new(0, wor_w, str_w, str);

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
    if ( 0x34d08717 != u3r_mug_words_new(str_w, wor_w) ) {
      fprintf(stderr, "fail (i) (3)\r\n");
      ret_i = 0;
    }
    if ( u3r_mug_words_new(str_w, 2) != u3r_mug_chub(str_d) ) {
      fprintf(stderr, "fail (i) (4)\r\n");
      ret_i = 0;
    }

    c3_free(str_y);
    c3_free(str_w);
    u3z(str);
  }

  {
    c3_w_new  som_w[4] = { 0, 0, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x519bd45c != u3r_mug(som) ) {
      fprintf(stderr, "fail (j) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x519bd45c != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "fail (j) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 0, 1, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x540eb8a9 != u3r_mug(som) ) {
      fprintf(stderr, "fail (k) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x540eb8a9 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "fail (k) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 1, 1, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x319d28f9 != u3r_mug(som) ) {
      fprintf(stderr, "fail (l) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x319d28f9 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "fail (l) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 0, 0, 0, 0xffff };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x5230a260 != u3r_mug(som) ) {
      fprintf(stderr, "fail (m) (1)\r\n");
      ret_i = 0;
    }

    if ( 0x5230a260 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "fail (m) (2)\r\n");
      ret_i = 0;
    }

    u3z(som);
  }

  //  32-bit and 64-bit boundary tests for direct/indirect atoms
  //
#ifdef VERE64
  //  64-bit mode: test boundary at 0x7fffffffffffffff and 0x8000000000000000
  {
    //  maximum direct atom value (63 bits set)
    //
    c3_y max_y[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f };
    u3_noun max = u3i_bytes(8, max_y);

    if ( c3y != u3a_is_cat(max) ) {
      fprintf(stderr, "fail (n) (1): max direct should be direct atom\r\n");
      ret_i = 0;
    }

    //  test mug on maximum direct atom
    //
    c3_m mug_m = u3r_mug(max);
    c3_m gum_m = u3r_mug_bytes(max_y, 8);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (n) (2): mug mismatch on max direct\r\n");
      ret_i = 0;
    }

    //  test that value equals u3a_direct_max
    //
    if ( max != u3a_direct_max ) {
      fprintf(stderr, "fail (n) (3): max direct value mismatch\r\n");
      ret_i = 0;
    }

    u3z(max);

    //  minimum indirect atom (bit 63 set)
    //
    c3_y min_y[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80 };
    u3_noun min = u3i_bytes(8, min_y);

    if ( c3y == u3a_is_cat(min) ) {
      fprintf(stderr, "fail (o) (1): min indirect should be indirect atom\r\n");
      ret_i = 0;
    }

    //  test mug on minimum indirect atom
    //
    mug_m = u3r_mug(min);
    gum_m = u3r_mug_bytes(min_y, 8);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (o) (2): mug mismatch on min indirect\r\n");
      ret_i = 0;
    }

    //  test word extraction across boundary
    //
    c3_w_new ext_w[2] = {0, 0};
    u3r_words_new(0, 2, ext_w, min);
    if ( 0x0 != ext_w[0] || 0x80000000 != ext_w[1] ) {
      fprintf(stderr, "fail (o) (3): word extraction mismatch\r\n");
      ret_i = 0;
    }

    u3z(min);
  }

  //  test atom at exactly 32-bit boundary
  {
    //  max 32-bit value, should be direct in 64-bit mode
    //
    c3_y max_y[4] = { 0xff, 0xff, 0xff, 0xff };
    u3_noun max = u3i_bytes(4, max_y);

    if ( c3y != u3a_is_cat(max) ) {
      fprintf(stderr, "fail (p) (1): 0xffffffff should be direct in 64-bit\r\n");
      ret_i = 0;
    }

    c3_m mug_32 = u3r_mug(max);
    c3_m mug_32_bytes = u3r_mug_bytes(max_y, 4);
    if ( mug_32 != mug_32_bytes ) {
      fprintf(stderr, "fail (p) (2): mug mismatch at 32-bit boundary\r\n");
      ret_i = 0;
    }

    u3z(max);

    //  just above 32-bit, should be direct in 64-bit mode
    //
    c3_y bov_y[5] = { 0x0, 0x0, 0x0, 0x0, 0x01 };
    u3_noun bov = u3i_bytes(5, bov_y);

    if ( c3y != u3a_is_cat(bov) ) {
      fprintf(stderr, "fail (p) (3): 0x100000000 should be direct in 64-bit\r\n");
      ret_i = 0;
    }

    c3_m mug_m = u3r_mug(bov);
    c3_m gum_m = u3r_mug_bytes(bov_y, 5);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (p) (4): mug mismatch above 32-bit\r\n");
      ret_i = 0;
    }

    u3z(bov);
  }
#else
  //  32-bit mode: test boundary at 0x7fffffff and 0x80000000
  {
    //  maximum direct atom value (31 bits set)
    //
    c3_y max_y[4] = { 0xff, 0xff, 0xff, 0x7f };
    u3_noun max = u3i_bytes(4, max_y);

    if ( c3y != u3a_is_cat(max) ) {
      fprintf(stderr, "fail (n) (1): max direct should be direct atom\r\n");
      ret_i = 0;
    }

    //  test mug on maximum direct atom
    //
    c3_m mug_m = u3r_mug(max);
    c3_m gum_m = u3r_mug_bytes(max_y, 4);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (n) (2): mug mismatch on max direct\r\n");
      ret_i = 0;
    }

    //  test that value equals u3a_direct_max
    //
    if ( u3a_direct_max != max ) {
      fprintf(stderr, "fail (n) (3): max direct value mismatch\r\n");
      ret_i = 0;
    }

    u3z(max);

    //  minimum indirect atom (bit 31 set)
    //
    c3_y min_y[4] = { 0x0, 0x0, 0x0, 0x80 };
    u3_noun min = u3i_bytes(4, min_y);

    if ( c3y == u3a_is_cat(min) ) {
      fprintf(stderr, "fail (o) (1): min indirect should be indirect atom\r\n");
      ret_i = 0;
    }

    //  test mug on minimum indirect atom
    //
    mug_m = u3r_mug(min);
    gum_m = u3r_mug_bytes(min_y, 4);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (o) (2): mug mismatch on min indirect\r\n");
      ret_i = 0;
    }

    //  test word extraction
    //
    c3_w_new rac_w = 0;
    u3r_words_new(0, 1, &rac_w, min);
    if ( 0x80000000 != rac_w ) {
      fprintf(stderr, "fail (o) (3): word extraction mismatch\r\n");
      ret_i = 0;
    }

    u3z(min);
  }
#endif

  //  test u3r_mug_words_new with boundary values (both modes)
  {
    //  test with array containing zero and maximum 32-bit value
    //
    c3_w_new bon_w[3] = { 0, 0xffffffff, 0 };
    u3_noun bon = u3i_words_new(3, bon_w);

    c3_m mug_m = u3r_mug(bon);
    c3_m gum_m = u3r_mug_words_new(bon_w, 3);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (q) (1): mug_words_new mismatch\r\n");
      ret_i = 0;
    }

    u3z(bon);

    //  test with single maximum 32-bit word
    //
    c3_w_new max_w[1] = { 0xffffffff };
    u3_noun max = u3i_words_new(1, max_w);

    mug_m = u3r_mug(max);
    gum_m = u3r_mug_words_new(max_w, 1);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "fail (q) (2): mug single word mismatch\r\n");
      ret_i = 0;
    }

    u3z(max);
  }

  return ret_i;
}


/* _test_met(): test u3r_met at direct atom boundaries
*/
static c3_i
_test_met(void)
{
  c3_i ret_i = 1;

  {
#ifdef VERE64
  u3_noun bon = u3i_note(0x7fffffffffffffffULL);
#else
  u3_noun bon = u3i_note(0x7fffffff);
#endif
  c3_n met_n = u3r_met(u3a_note_bits_log, bon);
  if ( 1 != met_n ) {
    fprintf(stderr, "fail (r) (1): met at boundary should be 1\r\n");
    ret_i = 0;
  }
  u3z(bon);
  }

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

  if ( !_test_met() ) {
    fprintf(stderr, "test_met: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_mug: ok\n");

  return 0;
}
