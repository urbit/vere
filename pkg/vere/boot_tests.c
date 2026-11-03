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

  u3C.wag_h |= u3o_hashless;
  u3m_boot_lite(1 << 27);
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
  c3_l lit_l;

  //  1: value too large should fail
  {
    c3_w big_w[] = {(c3_w)0, (c3_w)0, (c3_w)1};
    u3_noun big = u3i_words(3, big_w);
    u3_noun cod = u3dc("scot", c3__uv, big);

    if ( c3y == u3v_lily(c3__uv, cod, &lit_l) ) {
      printf("*** fail _test_lily-1\n");
      exit(1);
    }
  }

  //  2a: maximum direct atom value should succeed
  {
    u3_noun cod = u3dc("scot", c3__ud, u3a_direct_max);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (u3a_direct_max != lit_l) ) {
      printf("*** fail _test_lily-2a\n");
      exit(1);
    }
  }

  //  2b: value with indirect flag set should fail: "strange lily"
  {
    u3_noun cod = u3dc("scot", c3__ux, u3i_word(u3a_indirect_flag));
    if ( c3y == u3v_lily(c3__ux, cod, &lit_l) ) {
      printf("*** fail _test_lily-2b\n");
      exit(1);
    }
  }

  //  3: small values should work
  {
    u3_noun cod = u3dc("scot", c3__ud, 42);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (42 != lit_l) ) {
      printf("*** fail _test_lily-3\n");
      exit(1);
    }
  }

  //  4: zero should work
  {
    u3_noun cod = u3dc("scot", c3__ud, 0);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (0 != lit_l) ) {
      printf("*** fail _test_lily-4\n");
      exit(1);
    }
  }
}

/* _test_nest_memo(): the +dext:nest:ut memo must not leak hypotheses.
**
**   +nest compares two cores by assuming their pair nests while it checks
**   the arms.  A sub-result reached through that assumption is only valid
**   if the whole comparison succeeds; the jet used to cache it regardless,
**   so a failed comparison that did not crash (tel=|, or a member of a
**   %fork) made a later cast between the same intermediate types succeed.
**   Each case runs in one road, i.e. one transient memo cache.
*/
static void
_test_nest_memo()
{
  //  s0, r0: cores whose arm product mentions the core itself, so the
  //  comparison recurses into its own pair; it fails on the arm tail.
  //
  static const c3_c* dir_c =
    "=/  s0  -:!>(=>(~ |.([[. .] 0])))  "
    "=/  r0  -:!>(=>(~ |.([[. .] [0 0]])))  "
    "=/  one  (~(nest ut s0) | r0)  "
    "=/  two  (~(nest ut [%cell s0 s0]) | [%cell r0 r0])  "
    "=/  tri  (~(nest ut [%cell r0 r0]) | [%cell s0 s0])  "
    "=/  qua  (~(nest ut r0) | s0)  "
    "[one two tri qua]";
  //  compile-time: a cast to a %fork tries its members with tel=|;
  //  which member is tried first depends on type mugs, so both orders.
  //
  static const c3_c* pre_c =
    "=>  ~  =+  [a=|.([[. .] 0]) b=|.([[. .] [0 0]])]  ";
  static const struct { c3_c* src_c; c3_o ok_o; } cas_u[] = {
    { "=+  ^+(?:(& a b) b)  ^+(-:$:a -:$:b)", c3n },
    { "=+  ^+(?:(& a b) a)  ^+(-:$:b -:$:a)", c3n },
    { "^+(-:$:a -:$:b)",                      c3n },  //  unprimed
    { "=+  ^+(?:(& a b) b)  ^+(-:$:a -:$:a)", c3y },  //  control
  };
  c3_w i_w;

  {
    u3_noun pro = u3m_soft(0, u3v_wish_w, u3i_string(dir_c));
    u3_noun exp = u3nq(c3n, c3n, c3n, c3n);

    if ( (0 != u3h(pro)) || (c3n == u3r_sing(exp, u3t(pro))) ) {
      printf("*** fail _test_nest_memo-direct\n");
      exit(1);
    }
    u3z(exp);
    u3z(pro);
  }

  for ( i_w = 0; i_w < (sizeof(cas_u) / sizeof(*cas_u)); i_w++ ) {
    c3_c    src_c[256];
    u3_noun pro;

    snprintf(src_c, sizeof(src_c), "%s%s", pre_c, cas_u[i_w].src_c);
    pro = u3m_soft(0, u3v_wish_w, u3i_string(src_c));

    if ( (0 == u3h(pro)) != (c3y == cas_u[i_w].ok_o) ) {
      printf("*** fail _test_nest_memo-%u: %s\n", i_w, cas_u[i_w].src_c);
      exit(1);
    }
    u3z(pro);
  }
}

/* _test_ut_memo_fan(): the +ut memos must respect fan.
**
**   +redo keeps a hold that +rest is currently expanding (fan), and
**   expands it otherwise.  The jets used to cache its result regardless
**   of fan.  Each case computes a type in a road where the compiler has
**   already met the same +redo call under the other fan, and in a fresh
**   road, whose transient memo is empty.  In the first, (bar 0) is
**   compiled with vet, which expands its hold and refurbishes the
**   argument of (baz $) inside that expansion; (baz (bar 0)) is then
**   played outside it.  In the second, (baz (bar 0)) is played first,
**   outside the hold, and the hold is then expanded.
*/
static void
_test_ut_memo_fan()
{
  static const c3_c* pre_c =
    "=/  baz  |*(z=[a=* b=*] z)  "
    "=/  bar  |*(x=* ?:(& [x 0] [x (baz $)]))  "
    "=/  t  -:!>(.)  ";
  static const c3_c* cas_c[] = {
    "=/  h  -:!>((bar 0))  "
    "=/  g  (ream '(baz (bar 0))')  "
    "=/  in   (~(play ut t) g)  "
    "=/  out  +:(mule |.((~(play ut t) g)))  "
    "=(in out)",
    "=+  (~(play ut t) (ream '(baz (bar 0))'))  "
    "=/  h  (~(play ut t) (ream '(bar 0)'))  "
    "=/  in   ~(repo ut h)  "
    "=/  out  +:(mule |.(~(repo ut h)))  "
    "=(in out)",
  };
  c3_w i_w;

  for ( i_w = 0; i_w < (sizeof(cas_c) / sizeof(*cas_c)); i_w++ ) {
    c3_c    src_c[512];
    u3_noun pro;

    snprintf(src_c, sizeof(src_c), "%s%s", pre_c, cas_c[i_w]);
    pro = u3m_soft(0, u3v_wish_w, u3i_string(src_c));

    if ( (0 != u3h(pro)) || (c3y != u3t(pro)) ) {
      printf("*** fail _test_ut_memo_fan-%u\n", i_w);
      exit(1);
    }
    u3z(pro);
  }
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_lily();
  _test_nest_memo();
  _test_ut_memo_fan();

  fprintf(stderr, "test boot: ok\n");
  return 0;
}
