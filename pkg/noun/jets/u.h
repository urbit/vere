/// @file

#ifndef U3_JETS_U_H
#define U3_JETS_U_H

#include "types.h"

  /** u3u: jets with C-array arguments, for compiled nock.
  ***
  *** An array driver takes the arguments of an arm as an array of
  *** nouns, in the order of a preorder traversal of the arm's sample.
  *** Arguments are RETAINED, the product is transferred.  A driver
  *** produces u3_none to decline, like a u3w driver.
  **/
    typedef u3_weak (*u3u_fun)(u3_noun*);

  /* u3u_harm: array-driver arm.
  */
    typedef struct _u3u_harm {
      u3_noun     (*fun_f)(u3_noun);     //  u3w driver of the same arm
      u3u_fun       arg_f;               //  array driver
      c3_w          len_w;               //  number of arguments
      const c3_l*   axe_l;               //  argument axes in the core
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

  /** Tier 3.
  **/
    u3_noun u3uc_bex(u3_noun*);
    u3_noun u3uc_dvr(u3_noun*);

#endif /* ifndef U3_JETS_U_H */
