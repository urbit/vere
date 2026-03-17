/// @file

#include "noun.h"
#define TEST_SIZE 100000


/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 27);
  u3m_pave(c3y);
}

/* _test_put_del(): 
*/
static c3_i
_test_put_del()
{
  u3h_root har_u;
  u3h_new(&har_u);
  c3_i ret_i = 1;

  c3_w i_w;
  for ( i_w = 0; i_w < TEST_SIZE; i_w++ ) {
    u3_noun key = u3i_word(i_w);
    u3_noun val = u3nc(u3_nul, u3k(key));
    u3h_put(&har_u, key, val);
    u3z(key);
  }
  // fprintf(stderr, "inserted\r\n");

  for ( i_w = 0; i_w < TEST_SIZE; i_w++ ) {
    u3_noun key = u3i_word(i_w);
    u3_weak val = u3h_get(&har_u, key);
    if ( val == u3_none ) {
      fprintf(stderr, "failed insert\r\n");
      ret_i = 0;
    }
    u3z(key);
    u3z(val);
  }
  // fprintf(stderr, "presence\r\n");
  c3_w del_w[4] = {30, 82, 4921, 535};

  for ( i_w = 0; i_w < 4; i_w++ ) {
    u3_noun key = u3i_word(del_w[i_w]);
    u3h_del(&har_u, key);
    u3z(key);
  }
  // fprintf(stderr, "deleted\r\n");

  for ( i_w = 0; i_w < 4; i_w++ ) {
    u3_noun key = u3i_word(del_w[i_w]);
    u3_weak val = u3h_get(&har_u, key);
    if ( u3_none != val ) {
      fprintf(stderr, "failed delete\r\n");
      ret_i = 0;
      break;
    }
  }
  // fprintf(stderr, "presence two\r\n");
  u3h_free(&har_u);
  // fprintf(stderr, "freed\r\n");

  return ret_i;
}

/* _test_bit_manipulation():
*/
static c3_i
_test_bit_manipulation()
{
  c3_i ret_i = 1;

  if ( sizeof(u3_noun) != sizeof(u3h_slot) ) {
    fprintf(stderr, "bit manipulation: wrong size\r\n");
    ret_i = 0;
  }

  u3h_slot a = 0;

  a = u3h_slot_be_warm(a);
  if (u3h_slot_is_warm(a) != c3y) {
    fprintf(stderr, "bit manipulation: warmth\r\n");
    ret_i = 0;
  }

  a = u3h_slot_be_cold(a);
  if (u3h_slot_is_warm(a) != c3n) {
    fprintf(stderr, "bit manipulation: coldness\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _test_no_cache(): test a hashtable without caching.
*/
static c3_i
_test_no_cache(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 1000;
  c3_w   i_w;

  u3h_root har_u;
  u3h_new(&har_u);

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(&har_u, i_w, i_w + max_w);
  }

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    if ( (i_w + max_w) != u3h_get(&har_u, i_w) ) {
      fprintf(stderr, "bit test_no_cache: get failed\r\n");
      ret_i = 0;
    }
  }

  u3h_free(&har_u);
  return ret_i;
}

/* _test_cache_trimming(): ensure a caching hashtable removes stale items.
*/
static c3_i
_test_cache_trimming(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 2000000; // big number
  //c3_w max_w = 348000; // caused a leak before
  c3_w i_w, fil_w = max_w / 10;

  u3h_root har_u;
  u3h_new_cache(&har_u, fil_w);

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3_noun cel = u3nc(i_w, i_w);
    u3h_put(&har_u, cel, cel);
  }

  {
    // last thing we put in is still there
    c3_w  las_w = max_w - 1;
    u3_noun key = u3nc(las_w, las_w);
    u3_noun val = u3h_get(&har_u, key);
    u3z(key);

    if ( las_w != u3t(val) ) {
      fprintf(stderr, "cache_trimming (a): fail\r\n");
      ret_i = 0;
    }

    // if ( fil_w != har_u.use_w ) {
    //   fprintf(stderr, "cache_trimming (b): fail %d != %d\r\n",
    //           fil_w, har_u.use_w );
    //   ret_i = 0;
    // }

    u3z(val);
  }

  u3h_free(&har_u);
  return ret_i;
}

/* _test_cache_replace_value():
*/
static c3_i
_test_cache_replace_value(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 100;
  c3_w   i_w;

  u3h_root har_u;
  u3h_new_cache(&har_u, max_w);

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(&har_u, i_w, i_w + max_w);
  }

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(&har_u, i_w, i_w + max_w + 1);
  }

  if ( (2 * max_w) != u3h_get(&har_u, max_w - 1) ) {
    fprintf(stderr, "cache_replace (a): fail\r\n");
    ret_i = 0;
  }
  // if ( max_w != har_u->use_w ) {
  //   fprintf(stderr, "cache_replace (b): fail\r\n");
  //   fprintf(stderr, "cache_replace (b): fail %d %d\r\n",
  //           max_w, har_u->use_w );
  //   ret_i = 0;
  // }

  u3h_free(&har_u);
  return ret_i;
}

static c3_i
_test_hashtable(void)
{
  c3_i ret_i = 1;

  ret_i &= _test_bit_manipulation();
  fprintf(stderr, "_test_no_cache\r\n");
  ret_i &= _test_no_cache();
  fprintf(stderr, "_test_cache_trimming\r\n");
  ret_i &= _test_cache_trimming();
  fprintf(stderr, "_test_cache_replace_value\r\n");
  ret_i &= _test_cache_replace_value();
  fprintf(stderr, "_test_put_del\r\n");
  ret_i &= _test_put_del();

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_hashtable() ) {
    fprintf(stderr, "test_hashtable: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_hashtable: ok\r\n");

  return 0;
}
