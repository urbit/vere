/// @file

#include "ivory.h"
#include "noun.h"
#include "ur/ur.h"
#include "vere.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }
}

/* _test_lily(): test small noun parsing.
*/
static void
_test_lily()
{
  c3_l    lit_l;
  c3_w    big_w[] = {0, 0, 1};
  u3_noun big = u3i_words(3, big_w);
  u3_noun cod = u3dc("scot", c3__uv, big);

  if ( c3y == u3v_lily(c3__uv, cod, &lit_l) ) {
    printf("*** fail _test_lily-1\n");
    exit(1);
  }
  cod = u3dc("scot", c3__ud, 0x7fffffff);
  if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
       (0x7fffffff != lit_l) ) {
    printf("*** fail _test_lily-2a\n");
    exit(1);
  }
  cod = u3dc("scot", c3__ux, u3i_word(0x80000000));
  if ( c3y == u3v_lily(c3__ux, cod, &lit_l) ) {
    printf("*** fail _test_lily-2b\n");
    exit(1);
  }
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_lily();

  fprintf(stderr, "test boot: ok\n");
  return 0;
}
