/// @file

#ifndef U3_VORTEX_H
#define U3_VORTEX_H

#include "allocate.h"
#include "c3/c3.h"
#include "imprison.h"
#include "version.h"

  /**  Data structures.
  **/
    /* u3v_{32,64}_arvo, u3v_{32,64}_home: 32-bit and 64-bit image-state
    ** layouts.  Native u3v_arvo / u3v_home typedef-alias matching bitness.
    */
      typedef struct _u3v_32_arvo {
        c3_d        eve_d;
        u3_32_noun  roc;
        u3_32_noun  yot;
      } u3v_32_arvo;

      typedef struct _u3v_64_arvo {
        c3_d        eve_d;
        u3_64_noun  roc;
        u3_64_noun  yot;
      } u3v_64_arvo;

      typedef struct _u3v_32_home {
        u3v_version ver_d;
        c3_d        pam_d;
        u3v_32_arvo arv_u;
        u3a_32_road rod_u;
      } u3v_32_home;

      typedef struct _u3v_64_home {
        u3v_version ver_d;
        c3_d        pam_d;
        u3v_64_arvo arv_u;
        u3a_64_road rod_u;
      } u3v_64_home;

    /* u3v_arvo: modern arvo structure.
    ** u3v_home: all internal (within image) state.  NB: version must be
    ** first for ease of migration.
    */
#ifndef VERE64
      typedef u3v_32_arvo u3v_arvo;
      typedef u3v_32_home u3v_home;
#else
      typedef u3v_64_arvo u3v_arvo;
      typedef u3v_64_home u3v_home;
#endif

  /**  Globals.
  **/
      /// Arvo internal state.
      extern u3v_home* u3v_Home;
#       define u3H  u3v_Home
#       define u3A  (&(u3v_Home->arv_u))

      extern u3v_32_home* u3v_32_Home;
      extern u3v_64_home* u3v_64_Home;
      extern u3a_32_road* u3a_32_Road;
      extern u3a_64_road* u3a_64_Road;
#       define u3H_32  u3v_32_Home
#       define u3R_32  u3a_32_Road
#       define u3A_32  (&(u3v_32_Home->arv_u))
#       define u3H_64  u3v_64_Home
#       define u3R_64  u3a_64_Road
#       define u3A_64  (&(u3v_64_Home->arv_u))

  /**  Functions.
  **/
    /* u3v_life(): execute initial lifecycle, producing Arvo core.
    */
      u3_noun
      u3v_life(u3_noun eve);

    /* u3v_boot(): evaluate boot sequence, making a kernel
    */
      c3_o
      u3v_boot(u3_noun eve);

    /* u3v_boot_lite(): light bootstrap sequence, just making a kernel.
    */
      c3_o
      u3v_boot_lite(u3_noun lit);

    /* u3v_wish_w(): text expression with cache.
    */
      u3_noun
      u3v_wish_w(const u3_noun txt);

    /* u3v_do(): use a kernel function.
    */
      u3_noun
      u3v_do(const c3_c* txt_c, u3_noun arg);
#     define u3do(txt_c, a)          u3v_do(txt_c, a)
#     define u3dc(txt_c, a, b)       u3v_do(txt_c, u3nc(a, b))
#     define u3dt(txt_c, a, b, c)    u3v_do(txt_c, u3nt(a, b, c))
#     define u3dq(txt_c, a, b, c, d) u3v_do(txt_c, u3nq(a, b, c, d))

    /* u3v_wish(): text expression with cache.
    */
      u3_noun
      u3v_wish(const c3_c* str_c);

    /* u3v_lily(): parse little atom.
    */
      c3_o
      u3v_lily(u3_noun fot, u3_noun txt, c3_l* tid_l);

    /* u3v_peek(): query the reck namespace.
    */
      u3_noun
      u3v_peek(u3_noun hap);

    /* u3v_soft_peek(): softly query the reck namespace.
    */
      u3_noun
      u3v_soft_peek(c3_w mil_w, u3_noun sam);

    /* u3v_poke(): compute a timestamped ovum.
    */
      u3_noun
      u3v_poke(u3_noun sam);

    /* u3v_poke_sure(): inject an event, saving new state if successful.
    */
      c3_o
      u3v_poke_sure(c3_w mil_w, u3_noun eve, u3_noun* pro);

    /* u3v_tank(): dump single tank.
    */
      void
      u3v_tank(u3_noun blu, c3_l tab_l, u3_noun tac);

    /* u3v_punt(): dump tank list.
    */
      void
      u3v_punt(u3_noun blu, c3_l tab_l, u3_noun tac);

    /* u3v_sway(): print trace.
    */
      void
      u3v_sway(u3_noun blu, c3_l tab_l, u3_noun tax);

    /* u3v_plan(): queue ovum (external).
    */
      void
      u3v_plan(u3_noun pax, u3_noun fav);

    /* u3v_mark(): mark arvo kernel.
    */
      u3m_quac*
      u3v_mark();

    /* u3v_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3v_reclaim(void);

    /* u3v_rewrite_compact(): rewrite arvo kernel for compaction.
    */
      void
      u3v_rewrite_compact(void);

#endif /* ifndef U3_VORTEX_H */
