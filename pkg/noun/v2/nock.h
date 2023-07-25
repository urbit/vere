/// @file

#ifndef U3_NOCK_V2_H
#define U3_NOCK_V2_H

#include "pkg/noun/v3/nock.h"

#include "pkg/noun/v2/jets.h"

#include "types.h"

  /** Data structures.
  **/
    /* u3n_memo: %memo hint space
     */
    typedef struct {
      c3_l    sip_l;
      u3_noun key;
    } u3n_v2_memo;

  /* u3n_v2_prog: program compiled from nock
   */
  typedef struct _u3n_v2_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_w      len_w;                // number of literals
      u3_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_w      len_w;                // number of memo slots
      u3n_v2_memo* sot_u;             // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w      len_w;                // number of calls sites
      u3j_v2_site* sit_u;             // array of sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_v2_rite* rit_u;             // array of sites
    } reg_u;                          // registration site data
  } u3n_v2_prog;

  /**  Functions.
  **/
    /* u3n_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_v2_reclaim(void);

    /* u3n_v2_free(): free bytecode cache.
     */
      void
      u3n_v2_free(void);

    /* u3n_v2_mig_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_v2_mig_rewrite_compact();

#endif /* ifndef U3_NOCK_V2_H */
