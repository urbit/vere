/// @file

#include "noun.h"
#include "vere.h"
#include "ivory.h"
#include "ur/ur.h"
// #include "pact.h"


// defined in noun/hashtable.c
c3_w _ch_skip_slot(c3_w mug_w, c3_w lef_w);

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


/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  // if ( !_test_safe() ) {
  //   fprintf(stderr, "test unix: failed\r\n");
  //   exit(1);
  // }

  u3p(u3h_root) pit_p = u3h_new_cache(1000000);

  u3_noun init = u3nc(u3i_string("mess"),
                        u3nc(48,
                          u3nc(c3__pact,
                            u3nc(13105,
                              u3nc(c3__etch,
                                  u3nc(c3__init,
                                    u3nc(u3i_string("chum"),
                                      u3nc(49,
                                        u3nc(u3i_string("~nec"),
                                            u3nc(49,
                                              u3nc(u3i_string("0voctt.mbs2p.q9qaf.b17em.tbsit.49vat.7ggc2.8avm6.oe870.fqhkt.dltit.u54hf.lse3l.au3pt"), 0)
                                            )
                                          )
                                      ))))))));
  u3h_put(pit_p, init, u3nc(c3y, u3_nul));
  u3z(init);

  for ( c3_w fra_w = 0; fra_w < 100000; fra_w++ ) {
    u3_noun data = u3nc(u3i_string("mess"),
                          u3nc(48,
                            u3nc(c3__pact,
                              u3nc(13105,
                                u3nc(c3__etch,
                                    u3nc(c3__data,
                                      u3nc(u3dc("scot", c3__ud, fra_w),
                                      u3nc(u3i_string("chum"),
                                        u3nc(49,
                                          u3nc(u3i_string("~nec"),
                                              u3nc(49,
                                                u3nc(u3i_string("0voctt.mbs2p.q9qaf.b17em.tbsit.49vat.7ggc2.8avm6.oe870.fqhkt.dltit.u54hf.lse3l.au3pt"), 0)
                                              )
                                            )
                                        )
                                      )
                                    )
                                  )
                                )
                              )
                          )
                        )
                      );

    u3h_put(pit_p, data, u3nc(c3y, u3_nul));
    u3z(data);
  }

  for ( c3_w fra_w = 0; fra_w < 100000; fra_w++ ) {
    u3_noun data = u3nc(u3i_string("mess"),
                          u3nc(48,
                            u3nc(c3__pact,
                              u3nc(13105,
                                u3nc(c3__etch,
                                    u3nc(c3__data,
                                      u3nc(u3dc("scot", c3__ud, fra_w),
                                      u3nc(u3i_string("chum"),
                                        u3nc(49,
                                          u3nc(u3i_string("~nec"),
                                              u3nc(49,
                                                u3nc(u3i_string("0voctt.mbs2p.q9qaf.b17em.tbsit.49vat.7ggc2.8avm6.oe870.fqhkt.dltit.u54hf.lse3l.au3pt"), 0)
                                              )
                                            )
                                        )
                                      )
                                    )
                                  )
                                )
                              )
                          )
                        )
                      );
     u3_weak res = u3h_git(pit_p, data);
     if (res == u3_none) {
        fprintf(stderr, "key gone from hamt %u: not ok\r\n", fra_w);
        exit(1);
     }
     u3z(data);
  }

  u3h_free(pit_p);

  u3m_grab(u3_none);

  fprintf(stderr, "test unix: ok\r\n");
  return 0;
}
