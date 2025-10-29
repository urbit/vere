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
static void
_test_mug(void)
{
  c3_o fal_o = c3n;

  if ( 0x4d441035 != u3r_mug_c("Hello, world!") ) {
    fprintf(stderr, "_test_mug(): fail (a)\r\n");
    fal_o = c3y;
  }

  {
    u3_noun a = u3i_string("Hello, world!");

    if ( 0x4d441035 != u3r_mug(a) ) {
      fprintf(stderr, "_test_mug(): fail (b)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  {
    c3_y byt_y[1];

    if ( 0x79ff04e8 != u3r_mug_bytes(0, 0) ) {
      fprintf(stderr, "_test_mug(): fail (c) (0)\r\n");
      fal_o = c3y;
    }

    byt_y[0] = 1;

    if ( 0x715c2a60 != u3r_mug_bytes(byt_y, 1) ) {
      fprintf(stderr, "_test_mug(): fail (c) (1)\r\n");
      fal_o = c3y;
    }

    byt_y[0] = 2;

    if ( 0x718b9468 != u3r_mug_bytes(byt_y, 1) ) {
      fprintf(stderr, "_test_mug(): fail (c) (2)\r\n");
      fal_o = c3y;
    }
  }

  if ( 0x3a811aec != u3r_mug_both(0x715c2a60, u3r_mug_cell(2, 3)) ) {
    fprintf(stderr, "_test_mug(): fail (d)\r\n");
    fal_o = c3y;
  }


  {
    if ( 0x192f5588 != u3r_mug_cell(0, 0) ) {
      fprintf(stderr, "_test_mug(): fail (e) (1)\r\n");
      fal_o = c3y;
    }

    if ( 0x6b32ec46 != u3r_mug_cell(1, 1) ) {
      fprintf(stderr, "_test_mug(): fail (e) (2)\r\n");
      fal_o = c3y;
    }

    if ( 0x2effe10 != u3r_mug_cell(2, 2) ) {
      fprintf(stderr, "_test_mug(): fail (e) (3)\r\n");
      fal_o = c3y;
    }
  }

  {
    u3_noun a = u3i_string("xxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    if ( 0x64dfda5c != u3r_mug(a) ) {
      fprintf(stderr, "_test_mug(): fail (f)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  {
    u3_noun a = u3qc_bex(32);

    if ( 0x7cefb7f != u3r_mug_cell(0, a) ) {
      fprintf(stderr, "_test_mug(): fail (g)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  {
    u3_noun a = u3ka_dec(u3qc_bex(128));

    if ( 0x2aa06bfc != u3r_mug_cell(a, 1) ) {
      fprintf(stderr, "_test_mug(): fail (h)\r\n");
      fal_o = c3y;
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
    c3_w_new* str_w = c3_malloc(sizeof(c3_w_new) * wor_w);
    c3_d  str_d = c3y;

    u3r_bytes(0, byt_w, str_y, str);
    u3r_words_new(0, wor_w, str_w, str);

    str_d |= str_w[0];
    str_d |= ((c3_d)str_w[1] << 32ULL);

    if ( 0x34d08717 != u3r_mug(str) ) {
      fprintf(stderr, "_test_mug(): fail (i) (1) \r\n");
      fal_o = c3y;
    }
    if ( 0x34d08717 != u3r_mug_bytes(str_y, byt_w) ) {
      fprintf(stderr, "_test_mug(): fail (i) (2)\r\n");
      fal_o = c3y;
    }
    if ( 0x34d08717 != u3r_mug_words_new(str_w, wor_w) ) {
      fprintf(stderr, "_test_mug(): fail (i) (3)\r\n");
      fal_o = c3y;
    }
    if ( u3r_mug_words_new(str_w, 2) != u3r_mug_chub(str_d) ) {
      fprintf(stderr, "_test_mug(): fail (i) (4)\r\n");
      fal_o = c3y;
    }

    c3_free(str_y);
    c3_free(str_w);
    u3z(str);
  }

  {
    c3_w_new  som_w[4] = { 0, 0, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x519bd45c != u3r_mug(som) ) {
      fprintf(stderr, "_test_mug(): fail (j) (1)\r\n");
      fal_o = c3y;
    }

    if ( 0x519bd45c != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "_test_mug(): fail (j) (2)\r\n");
      fal_o = c3y;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 0, 1, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x540eb8a9 != u3r_mug(som) ) {
      fprintf(stderr, "_test_mug(): fail (k) (1)\r\n");
      fal_o = c3y;
    }

    if ( 0x540eb8a9 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "_test_mug(): fail (k) (2)\r\n");
      fal_o = c3y;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 1, 1, 0, 1 };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x319d28f9 != u3r_mug(som) ) {
      fprintf(stderr, "_test_mug(): fail (l) (1)\r\n");
      fal_o = c3y;
    }

    if ( 0x319d28f9 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "_test_mug(): fail (l) (2)\r\n");
      fal_o = c3y;
    }

    u3z(som);
  }

  {
    c3_w_new  som_w[4] = { 0, 0, 0, 0xffff };
    u3_noun som    = u3i_words_new(4, som_w);

    if ( 0x5230a260 != u3r_mug(som) ) {
      fprintf(stderr, "_test_mug(): fail (m) (1)\r\n");
      fal_o = c3y;
    }

    if ( 0x5230a260 != u3r_mug_words_new(som_w, 4) ) {
      fprintf(stderr, "_test_mug(): fail (m) (2)\r\n");
      fal_o = c3y;
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
      fprintf(stderr, "_test_mug(): fail (n) (1): max direct should be direct atom\r\n");
      fal_o = c3y;
    }

    //  test mug on maximum direct atom
    //
    c3_m mug_m = u3r_mug(max);
    c3_m gum_m = u3r_mug_bytes(max_y, 8);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (n) (2): mug mismatch on max direct\r\n");
      fal_o = c3y;
    }

    //  test that value equals u3a_direct_max
    //
    if ( max != u3a_direct_max ) {
      fprintf(stderr, "_test_mug(): fail (n) (3): max direct value mismatch\r\n");
      fal_o = c3y;
    }

    u3z(max);

    //  minimum indirect atom (bit 63 set)
    //
    c3_y min_y[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80 };
    u3_noun min = u3i_bytes(8, min_y);

    if ( c3y == u3a_is_cat(min) ) {
      fprintf(stderr, "_test_mug(): fail (o) (1): min indirect should be indirect atom\r\n");
      fal_o = c3y;
    }

    //  test mug on minimum indirect atom
    //
    mug_m = u3r_mug(min);
    gum_m = u3r_mug_bytes(min_y, 8);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (o) (2): mug mismatch on min indirect\r\n");
      fal_o = c3y;
    }

    //  test word extraction across boundary
    //
    c3_w_new ext_w[2] = {0, 0};
    u3r_words_new(0, 2, ext_w, min);
    if ( 0x0 != ext_w[0] || 0x80000000 != ext_w[1] ) {
      fprintf(stderr, "_test_mug(): fail (o) (3): word extraction mismatch\r\n");
      fal_o = c3y;
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
      fprintf(stderr, "_test_mug(): fail (p) (1): 0xffffffff should be direct in 64-bit\r\n");
      fal_o = c3y;
    }

    c3_m mug_m = u3r_mug(max);
    c3_m gum_m = u3r_mug_bytes(max_y, 4);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (p) (2): mug mismatch at 32-bit boundary\r\n");
      fal_o = c3y;
    }

    u3z(max);

    //  just above 32-bit, should be direct in 64-bit mode
    //
    c3_y bov_y[5] = { 0x0, 0x0, 0x0, 0x0, 0x01 };
    u3_noun bov = u3i_bytes(5, bov_y);

    if ( c3y != u3a_is_cat(bov) ) {
      fprintf(stderr, "_test_mug(): fail (p) (3): 0x100000000 should be direct in 64-bit\r\n");
      fal_o = c3y;
    }

    mug_m = u3r_mug(bov);
    gum_m = u3r_mug_bytes(bov_y, 5);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (p) (4): mug mismatch above 32-bit\r\n");
      fal_o = c3y;
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
      fprintf(stderr, "_test_mug(): fail (n) (1): max direct should be direct atom\r\n");
      fal_o = c3y;
    }

    //  test mug on maximum direct atom
    //
    c3_m mug_m = u3r_mug(max);
    c3_m gum_m = u3r_mug_bytes(max_y, 4);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (n) (2): mug mismatch on max direct\r\n");
      fal_o = c3y;
    }

    //  test that value equals u3a_direct_max
    //
    if ( u3a_direct_max != max ) {
      fprintf(stderr, "_test_mug(): fail (n) (3): max direct value mismatch\r\n");
      fal_o = c3y;
    }

    u3z(max);

    //  minimum indirect atom (bit 31 set)
    //
    c3_y min_y[4] = { 0x0, 0x0, 0x0, 0x80 };
    u3_noun min = u3i_bytes(4, min_y);

    if ( c3y == u3a_is_cat(min) ) {
      fprintf(stderr, "_test_mug(): fail (o) (1): min indirect should be indirect atom\r\n");
      fal_o = c3y;
    }

    //  test mug on minimum indirect atom
    //
    mug_m = u3r_mug(min);
    gum_m = u3r_mug_bytes(min_y, 4);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (o) (2): mug mismatch on min indirect\r\n");
      fal_o = c3y;
    }

    //  test word extraction
    //
    c3_w_new rac_w = c3y;
    u3r_words_new(0, 1, &rac_w, min);
    if ( 0x80000000 != rac_w ) {
      fprintf(stderr, "_test_mug(): fail (o) (3): word extraction mismatch\r\n");
      fal_o = c3y;
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
      fprintf(stderr, "_test_mug(): fail (q) (1): mug_words_new mismatch\r\n");
      fal_o = c3y;
    }

    u3z(bon);

    //  test with single maximum 32-bit word
    //
    c3_w_new max_w[1] = { 0xffffffff };
    u3_noun max = u3i_words_new(1, max_w);

    mug_m = u3r_mug(max);
    gum_m = u3r_mug_words_new(max_w, 1);
    if ( mug_m != gum_m ) {
      fprintf(stderr, "_test_mug(): fail (q) (2): mug single word mismatch\r\n");
      fal_o = c3y;
    }

    u3z(max);
  }
  if __(fal_o)
    exit(1);
}

/* _test_at(): test fragment extraction with u3r_at().
*/
static void
_test_at(void)
{
  c3_o fal_o = c3n;

  //  axis 0 should return u3_none
  //
  {
    u3_noun a = u3nc(1, 2);
    if ( u3_none != u3r_at(0, a) ) {
      fprintf(stderr, "_test_at(): fail (a)\r\n");
      fal_o = c3y;
    }
    u3z(a);
  }

  //  axis 1 is identity
  //
  {
    u3_noun a = u3nc(1, 2);
    u3_noun fag = u3r_at(1, a);
    if ( a != fag ) {
      fprintf(stderr, "_test_at(): fail (b)\r\n");
      fal_o = c3y;
    }
    u3z(a);
  }

  //  axis 2 is head, axis 3 is tail
  //
  {
    u3_noun a = u3nc(42, 99);
    u3_noun p = u3r_at(2, a);
    u3_noun q = u3r_at(3, a);
    if ( 42 != p || 99 != q ) {
      fprintf(stderr, "_test_at(): fail (c)\r\n");
      fal_o = c3y;
    }
    u3z(a);
  }

  //  test invalid fragment (atom instead of cell)
  //
  {
    if ( u3_none != u3r_at(2, 42) ) {
      fprintf(stderr, "_test_at(): fail (d)\r\n");
      fal_o = c3y;
    }
  }

  //  test deep fragment traversal
  //
  {
    //  [[1 2] [3 4]]
    u3_noun a = u3nc(u3nc(1, 2), u3nc(3, 4));

    //  axis 4 = head of head = 1
    if ( 1 != u3r_at(4, a) ) {
      fprintf(stderr, "_test_at(): fail (e) (1)\r\n");
      fal_o = c3y;
    }

    //  axis 5 = tail of head = 2
    if ( 2 != u3r_at(5, a) ) {
      fprintf(stderr, "_test_at(): fail (e) (2)\r\n");
      fal_o = c3y;
    }

    //  axis 6 = head of tail = 3
    if ( 3 != u3r_at(6, a) ) {
      fprintf(stderr, "_test_at(): fail (e) (3)\r\n");
      fal_o = c3y;
    }

    //  axis 7 = tail of tail = 4
    if ( 4 != u3r_at(7, a) ) {
      fprintf(stderr, "_test_at(): fail (e) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test with maximum direct atom as axis
  //
  {
    //  create a large balanced tree
    u3_noun tee = u3nc(1, 2);
    c3_y i_y;
    for ( i_y = c3y; i_y < 10; i_y++ ) {
      tee = u3nc(tee, u3nc(i_y, i_y + 1));
    }

    //  test various axes work
    if ( u3_none == u3r_at(2, tee) ) {
      fprintf(stderr, "_test_at(): fail (f)\r\n");
      fal_o = c3y;
    }

    u3z(tee);
  }

  if _(fal_o)
    exit(1);
}

/* _test_bit_byte(): test u3r_bit() and u3r_byte().
*/
static void
_test_bit_byte(void)
{
  c3_o fal_o = c3n;

  //  test bit extraction from direct atom
  //
  {
    //  0b1011 = 11
    u3_noun a = 11;

    if ( 1 != u3r_bit(0, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (a) (1)\r\n");
      fal_o = c3y;
    }
    if ( 1 != u3r_bit(1, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (a) (2)\r\n");
      fal_o = c3y;
    }
    if ( 0 != u3r_bit(2, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (a) (3)\r\n");
      fal_o = c3y;
    }
    if ( 1 != u3r_bit(3, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (a) (4)\r\n");
      fal_o = c3y;
    }
    //  out of bounds should return 0
    if ( 0 != u3r_bit(100, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (a) (5)\r\n");
      fal_o = c3y;
    }
  }

  //  test byte extraction from direct atom
  //
  {
    //  0x12345678
    u3_noun a = 0x12345678;

    if ( 0x78 != u3r_byte(0, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (b) (1)\r\n");
      fal_o = c3y;
    }
    if ( 0x56 != u3r_byte(1, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (b) (2)\r\n");
      fal_o = c3y;
    }
    if ( 0x34 != u3r_byte(2, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (b) (3)\r\n");
      fal_o = c3y;
    }
    if ( 0x12 != u3r_byte(3, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (b) (4)\r\n");
      fal_o = c3y;
    }
    //  out of bounds should return 0
    if ( 0 != u3r_byte(10, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (b) (5)\r\n");
      fal_o = c3y;
    }
  }

  //  test bit/byte extraction at word boundaries
  //
  {
#ifdef VERE64
    //  test at bit 31 and 32 (32-bit word boundary) in 64-bit mode
    u3_noun a = u3i_chub(0x100000000ULL);  //  bit 32 set

    if ( 0 != u3r_bit(31, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (c) (1) 64-bit\r\n");
      fal_o = c3y;
    }
    if ( 1 != u3r_bit(32, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (c) (2) 64-bit\r\n");
      fal_o = c3y;
    }

    u3z(a);
#else
    //  test at bit 30 and 31 in 32-bit mode
    u3_noun a = u3i_word(0x80000000);  //  bit 31 set (indirect atom)

    if ( 0 != u3r_bit(30, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (c) (1) 32-bit\r\n");
      fal_o = c3y;
    }
    if ( 1 != u3r_bit(31, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (c) (2) 32-bit\r\n");
      fal_o = c3y;
    }

    u3z(a);
#endif
  }

  //  test extraction from indirect atom
  //
  {
    c3_y buf_y[10] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a };
    u3_noun a = u3i_bytes(10, buf_y);

    //  extract bytes
    if ( 0x01 != u3r_byte(0, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (d) (1)\r\n");
      fal_o = c3y;
    }
    if ( 0x05 != u3r_byte(4, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (d) (2)\r\n");
      fal_o = c3y;
    }
    if ( 0x0a != u3r_byte(9, a) ) {
      fprintf(stderr, "_test_bit_byte(): fail (d) (3)\r\n");
      fal_o = c3y;
    }

    //  extract bits
    if ( 1 != u3r_bit(0, a) ) {  //  LSB of 0x01
      fprintf(stderr, "_test_bit_byte(): fail (d) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  if _(fal_o)
    exit(1);
}

/* _test_bytes(): test u3r_bytes() byte range copying.
*/
static void
_test_bytes(void)
{
  c3_o fal_o = c3n;

  //  test copying from direct atom
  //
  {
    u3_noun a = 0x12345678;
    c3_y buf_y[4];

    u3r_bytes(0, 4, buf_y, a);

    if (  0x78 != buf_y[0]
       || 0x56 != buf_y[1]
       || 0x34 != buf_y[2]
       || 0x12 != buf_y[3] )
    {
      fprintf(stderr, "_test_bytes(): fail (a)\r\n");
      fal_o = c3y;
    }
  }

  //  test copying from indirect atom
  //
  {
    c3_y src_y[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    u3_noun a = u3i_bytes(8, src_y);
    c3_y buf_y[8];

    u3r_bytes(0, 8, buf_y, a);

    if ( 0 != memcmp(buf_y, src_y, 8) ) {
      fprintf(stderr, "_test_bytes(): fail (b)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test offset copying
  //
  {
    c3_y src_y[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    u3_noun a = u3i_bytes(8, src_y);
    c3_y buf_y[4];

    //  copy bytes 2-5
    u3r_bytes(2, 4, buf_y, a);

    if (  0x03 != buf_y[0]
       || 0x04 != buf_y[1]
       || 0x05 != buf_y[2]
       || 0x06 != buf_y[3] )
    {
      fprintf(stderr, "_test_bytes(): fail (c)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test zero-padding for out-of-bounds read
  //
  {
    u3_noun a = 0xff;  //  1 byte
    c3_y buf_y[4] = { 0xaa, 0xaa, 0xaa, 0xaa };  //  init with non-zero

    u3r_bytes(0, 4, buf_y, a);

    if (  0xff != buf_y[0]
       || 0x00 != buf_y[1]
       || 0x00 != buf_y[2]
       || 0x00 != buf_y[3] )
    {
      fprintf(stderr, "_test_bytes(): fail (d)\r\n");
      fal_o = c3y;
    }
  }

  //  test crossing word boundaries
  //
  {
#ifdef VERE64
    //  8-byte atom that crosses word boundary when extracted as words
    c3_y src_y[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };
#else
    //  4-byte atom
    c3_y src_y[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
#endif
    u3_noun a = u3i_bytes(sizeof(src_y), src_y);
    c3_y buf_y[sizeof(src_y)];

    u3r_bytes(0, sizeof(src_y), buf_y, a);

    if ( 0 != memcmp(buf_y, src_y, sizeof(src_y)) ) {
      fprintf(stderr, "_test_bytes(): fail (e) boundary\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  if _(fal_o)
    exit(1);
}

/* _test_words(): test u3r_word_new(), u3r_chub(), u3r_word().
*/
static void
_test_words(void)
{
  c3_o fal_o = c3n;

  //  test u3r_word_new() extraction
  //
  {
    c3_w_new words_w[3] = { 0x12345678, 0xaabbccdd, 0xdeadbeef };
    u3_noun a = u3i_words_new(3, words_w);

    if ( 0x12345678 != u3r_word_new(0, a) ) {
      fprintf(stderr, "_test_words(): fail (a) (1)\r\n");
      fal_o = c3y;
    }
    if ( 0xaabbccdd != u3r_word_new(1, a) ) {
      fprintf(stderr, "_test_words(): fail (a) (2)\r\n");
      fal_o = c3y;
    }
    if ( 0xdeadbeef != u3r_word_new(2, a) ) {
      fprintf(stderr, "_test_words(): fail (a) (3)\r\n");
      fal_o = c3y;
    }
    //  out of bounds should return 0
    if ( 0 != u3r_word_new(10, a) ) {
      fprintf(stderr, "_test_words(): fail (a) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test u3r_chub() extraction
  //
  {
    c3_d cub_d[2] = { 0x123456789abcdef0ULL, 0xfedcba9876543210ULL };
    u3_noun a = u3i_chubs(2, cub_d);

    if ( 0x123456789abcdef0ULL != u3r_chub(0, a) ) {
      fprintf(stderr, "_test_words(): fail (b) (1)\r\n");
      fal_o = c3y;
    }
    if ( 0xfedcba9876543210ULL != u3r_chub(1, a) ) {
      fprintf(stderr, "_test_words(): fail (b) (2)\r\n");
      fal_o = c3y;
    }
    //  out of bounds should return 0
    if ( 0 != u3r_chub(10, a) ) {
      fprintf(stderr, "_test_words(): fail (b) (3)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test u3r_word() returns correct size based on architecture
  //
  {
#ifdef VERE64
    c3_w not_w[2] = { 0x123456789abcdef0ULL, 0xfedcba9876543210ULL };
    u3_noun a = u3i_words(2, not_w);

    if ( 0x123456789abcdef0ULL != u3r_word(0, a) ) {
      fprintf(stderr, "_test_words(): fail (c) (1) 64-bit\r\n");
      fal_o = c3y;
    }
    if ( 0xfedcba9876543210ULL != u3r_word(1, a) ) {
      fprintf(stderr, "_test_words(): fail (c) (2) 64-bit\r\n");
      fal_o = c3y;
    }
#else
    c3_w not_w[2] = { 0x12345678, 0xaabbccdd };
    u3_noun a = u3i_words(2, not_w);

    if ( 0x12345678 != u3r_word(0, a) ) {
      fprintf(stderr, "_test_words(): fail (c) (1) 32-bit\r\n");
      fal_o = c3y;
    }
    if ( 0xaabbccdd != u3r_word(1, a) ) {
      fprintf(stderr, "_test_words(): fail (c) (2) 32-bit\r\n");
      fal_o = c3y;
    }
#endif

    u3z(a);
  }

  //  test extraction at 32/64-bit boundaries
  //
  {
#ifdef VERE64
    //  in 64-bit mode, test extracting 32-bit words from 64-bit atom
    u3_noun a = u3i_chub(0x123456789abcdef0ULL);

    //  should extract lower 32 bits
    if ( 0x9abcdef0 != u3r_word_new(0, a) ) {
      fprintf(stderr, "_test_words(): fail (d) (1) 64-bit\r\n");
      fal_o = c3y;
    }
    //  should extract upper 32 bits
    if ( 0x12345678 != u3r_word_new(1, a) ) {
      fprintf(stderr, "_test_words(): fail (d) (2) 64-bit\r\n");
      fal_o = c3y;
    }

    u3z(a);
#endif
  }

  if _(fal_o)
    exit(1);
}

/* _test_safe(): test u3r_safe_*() validation functions.
*/
static void
_test_safe(void)
{
  c3_o fal_o = c3n;

  //  test u3r_safe_byte()
  //
  {
    c3_y val_y;

    //  should succeed for values 0-255
    if ( c3n == u3r_safe_byte(42, &val_y) || 42 != val_y ) {
      fprintf(stderr, "_test_safe(): fail (a) (1)\r\n");
      fal_o = c3y;
    }
    if ( c3n == u3r_safe_byte(255, &val_y) || 255 != val_y ) {
      fprintf(stderr, "_test_safe(): fail (a) (2)\r\n");
      fal_o = c3y;
    }

    //  should fail for values > 255
    if ( c3y == u3r_safe_byte(256, &val_y) ) {
      fprintf(stderr, "_test_safe(): fail (a) (3)\r\n");
      fal_o = c3y;
    }

    //  should fail for cells
    u3_noun a = u3nc(1, 2);
    if ( c3y == u3r_safe_byte(a, &val_y) ) {
      fprintf(stderr, "_test_safe(): fail (a) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test u3r_safe_word_new()
  //
  {
    c3_w_new val_w;

    //  should succeed for 32-bit values
    if ( c3n == u3r_safe_word_new(0x12345678, &val_w) || 0x12345678 != val_w ) {
      fprintf(stderr, "_test_safe(): fail (b) (1)\r\n");
      fal_o = c3y;
    }
    if ( c3n == u3r_safe_word_new(0x7fffffff, &val_w) || 0x7fffffff != val_w ) {
      fprintf(stderr, "_test_safe(): fail (b) (2)\r\n");
      fal_o = c3y;
    }

    //  should fail for values > 32 bits
    {
      c3_y big_y[5] = { 0x00, 0x00, 0x00, 0x00, 0x01 };
      u3_noun big = u3i_bytes(5, big_y);
      if ( c3y == u3r_safe_word_new(big, &val_w) ) {
        fprintf(stderr, "_test_safe(): fail (b) (3)\r\n");
        fal_o = c3y;
      }
      u3z(big);
    }

    //  should fail for cells
    u3_noun a = u3nc(1, 2);
    if ( c3y == u3r_safe_word_new(a, &val_w) ) {
      fprintf(stderr, "_test_safe(): fail (b) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test u3r_safe_chub()
  //
  {
    c3_d val_d;

    //  should succeed for 64-bit values
    c3_y val_y[8] = { 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 };
    u3_noun val = u3i_bytes(8, val_y);
    if (  c3n == u3r_safe_chub(val, &val_d) || 0x123456789abcdef0ULL != val_d ) {
      fprintf(stderr, "_test_safe(): fail (c) (1)\r\n");
      fal_o = c3y;
    }

    //  should fail for values > 64 bits
    {
      c3_y big_y[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
      u3_noun big = u3i_bytes(9, big_y);
      if ( c3y == u3r_safe_chub(big, &val_d) ) {
        fprintf(stderr, "_test_safe(): fail (c) (2)\r\n");
        fal_o = c3y;
      }
      u3z(big);
    }

    //  should fail for cells
    u3_noun a = u3nc(1, 2);
    if ( c3y == u3r_safe_chub(a, &val_d) ) {
      fprintf(stderr, "_test_safe(): fail (c) (3)\r\n");
      fal_o = c3y;
    }

    u3z(a);
    u3z(val);
  }

  //  test u3r_safe_word() validates per architecture
  //
  {
    c3_w val_w;

#ifdef VERE64
    //  64-bit mode: should work like u3r_safe_chub
    if (  c3n == u3r_safe_word(0x123456789abcdef0ULL, &val_w)
       || 0x123456789abcdef0ULL != val_w )
    {
      fprintf(stderr, "_test_safe(): fail (d) 64-bit\r\n");
      fal_o = c3y;
    }
#else
    //  32-bit mode: should work like u3r_safe_word_new
    if ( c3n == u3r_safe_word(0x12345678, &val_w) || 0x12345678 != val_w ) {
      fprintf(stderr, "_test_safe(): fail (d) 32-bit\r\n");
      fal_o = c3y;
    }
#endif
  }

  if _(fal_o)
    exit(1);
}

/* _test_cell_trel_qual(): test tuple factoring functions.
*/
static void
_test_cell_trel_qual(void)
{
  c3_o fal_o = c3n;

  //  test u3r_cell()
  //
  {
    u3_noun a = u3nc(42, 99);
    u3_noun p, q;

    if ( c3n == u3r_cell(a, &p, &q) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (a) (1)\r\n");
      fal_o = c3y;
    }
    if ( 42 != p || 99 != q ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (a) (2)\r\n");
      fal_o = c3y;
    }

    //  should fail on atom
    if ( c3y == u3r_cell(42, &p, &q) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (a) (3)\r\n");
      fal_o = c3y;
    }

    //  test with NULL pointers (should still succeed)
    if ( c3n == u3r_cell(a, NULL, NULL) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (a) (4)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  //  test u3r_trel()
  //
  {
    u3_noun a = u3nt(1, 2, 3);
    u3_noun p, q, r;

    if ( c3n == u3r_trel(a, &p, &q, &r) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (b) (1)\r\n");
      fal_o = c3y;
    }
    if ( 1 != p || 2 != q || 3 != r ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (b) (2)\r\n");
      fal_o = c3y;
    }

    //  should fail on non-trel
    u3_noun b = u3nc(1, 2);
    if ( c3y == u3r_trel(b, &p, &q, &r) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (b) (3)\r\n");
      fal_o = c3y;
    }

    u3z(a);
    u3z(b);
  }

  //  test u3r_qual()
  //
  {
    u3_noun a = u3nq(1, 2, 3, 4);
    u3_noun p, q, r, s;

    if ( c3n == u3r_qual(a, &p, &q, &r, &s) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (c) (1)\r\n");
      fal_o = c3y;
    }
    if ( 1 != p || 2 != q || 3 != r || 4 != s ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (c) (2)\r\n");
      fal_o = c3y;
    }

    //  should fail on non-qual
    u3_noun b = u3nt(1, 2, 3);
    if ( c3y == u3r_qual(b, &p, &q, &r, &s) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (c) (3)\r\n");
      fal_o = c3y;
    }

    u3z(a);
    u3z(b);
  }

  //  test with maximum direct atoms at boundaries
  //
  {
    u3_noun max = u3a_direct_max;
    u3_noun a = u3nc(max, max);
    u3_noun p, q;

    if ( c3n == u3r_cell(a, &p, &q) ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (d) (1)\r\n");
      fal_o = c3y;
    }
    if ( max != p || max != q ) {
      fprintf(stderr, "_test_cell_trel_qual(): fail (d) (2)\r\n");
      fal_o = c3y;
    }

    u3z(a);
  }

  if _(fal_o)
    exit(1);
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_mug();
  _test_at();
  _test_bit_byte();
  _test_bytes();
  _test_words();
  _test_safe();
  _test_cell_trel_qual();

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "retrieve_tests: ok\n");

  return 0;
}
