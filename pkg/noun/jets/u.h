/// @file

#ifndef U3_JETS_U_H
#define U3_JETS_U_H

#include "types.h"

  /** u3u: jets with C-array arguments, for compiled nock.
  ***
  *** An array driver takes the arguments of an arm as an array of
  *** nouns, in depth-first order (see +flatten-need in nock-compilation.hoon)
  *** Arguments are RETAINED, the product is transferred.  Array drivers
  *** are total: they produce or bail, and never punt.  An arm with no
  *** array driver may be listed as punting instead: compiled nock then
  *** calls its u3w driver on the whole core, and the nock of the arm if
  *** the driver punts.
  **/
    typedef u3_noun (*u3u_fun)(u3_noun*);

  /* u3u_harm: array-driver arm.
  **    key_f: a key to map this entry to the hot state, u3w* driver in practice
  **    arg_f: array driver, NULL if punting
  **    len_w: number of arguments, 1 if punting
  **    axe_l: argument axes in the core, {1} if punting
  **    pun_t: punting driver, u3w* driver from the hot state will apply
  */
    typedef struct _u3u_harm {
      u3_noun     (*key_f)(u3_noun);
      u3u_fun       arg_f;
      c3_w          len_w;
      const c3_l*   axe_l;
      c3_t          pun_t;
    } u3u_harm;

  /* u3u_Harm: blank-terminated table of array drivers.
  */
    extern const u3u_harm u3u_Harm[];

  /** Tier 1.
  **/
    u3_noun u3ua_add(u3_noun*);
    u3_noun u3ua_dec(u3_noun*);
    u3_noun u3ua_div(u3_noun*);
    u3_noun u3ua_gte(u3_noun*);
    u3_noun u3ua_gth(u3_noun*);
    u3_noun u3ua_lte(u3_noun*);
    u3_noun u3ua_lth(u3_noun*);
    u3_noun u3ua_max(u3_noun*);
    u3_noun u3ua_min(u3_noun*);
    u3_noun u3ua_mod(u3_noun*);
    u3_noun u3ua_mul(u3_noun*);
    u3_noun u3ua_sub(u3_noun*);

  /** Tier 2.
  **/
    u3_noun u3ub_reap(u3_noun*);
    u3_noun u3ub_snag(u3_noun*);
    u3_noun u3ub_weld(u3_noun*);

  /** Tier 3.
  **/
    u3_noun u3uc_bex(u3_noun*);
    u3_noun u3uc_cat(u3_noun*);
    u3_noun u3uc_con(u3_noun*);
    u3_noun u3uc_cut(u3_noun*);
    u3_noun u3uc_dis(u3_noun*);
    u3_noun u3uc_dvr(u3_noun*);
    u3_noun u3uc_end(u3_noun*);
    u3_noun u3uc_lsh(u3_noun*);
    u3_noun u3uc_met(u3_noun*);
    u3_noun u3uc_mix(u3_noun*);
    u3_noun u3uc_muk(u3_noun*);
    u3_noun u3uc_rap(u3_noun*);
    u3_noun u3uc_rip(u3_noun*);
    u3_noun u3uc_rsh(u3_noun*);
    u3_noun u3ucp_tod(u3_noun*);
    u3_noun u3ucp_tos(u3_noun*);

  /** Tier 5.
  **/
    u3_noun u3ue_fein_ob(u3_noun*);
    u3_noun u3ue_fynd_ob(u3_noun*);
    u3_noun u3ue_jam(u3_noun*);
    u3_noun u3ue_trip(u3_noun*);

#endif /* ifndef U3_JETS_U_H */
