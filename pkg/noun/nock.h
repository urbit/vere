/// @file

#ifndef U3_NOCK_H
#define U3_NOCK_H

#include <stdio.h>

#include "c3/c3.h"
#include "jets.h"
#include "types.h"
#include "zave.h"

#if 0
  |%
  +$  cape  $~(| $@(? (pair cape cape)))
  +$  sock  $~(|+~ (pair cape *))
  --
#endif

  /** Data structures.
  ***
  **/

  /* u3n_memo: %memo hint space
   */
  typedef struct {
    c3_l    sip_l;
    u3_noun key;
    u3z_cid cid;
  } u3n_memo;

  /* u3n_dire: direct call information
   */
  struct _u3n_prog;
  typedef struct {
    u3_noun         bell;    //  [sock formula]
    u3p(_u3n_prog)  pog_p;   //  called program post or [less formula] during compilation
    u3j_harm*       ham_u;   //  jet arm, nullable
    c3_l            axe_l;   //  jet arm axis
  } u3n_dire;

  /* u3n_prog: program compiled from nock
  ** u3n_prog is read/write if it is a regular program
  ** and it is READ-ONLY if it is a program with direct calls
  ** the read/write portions are cal_u and reg_u, they MUST be
  ** empty if dir_u is not empty
  **
  ** Regular programs have to be copied onto the current road to
  ** execute them, read-only programs don't need to be copied.
  */
  typedef struct _u3n_prog {
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
      u3n_memo* sot_u;                // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w      len_w;                // number of calls sites
      u3j_site* sit_u;                // array of sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_rite* rit_u;                // array of sites
    } reg_u;                          // registration site data
    struct {
      c3_w      len_w;                // number of direct calls
      u3n_dire* dat_u;                // array of call info
    } dir_u;                          // direct call data
  } u3n_prog;

  /**  Functions.
  **/
    /* u3n_nock_on(): produce .*(bus fol).
    */
      u3_noun
      u3n_nock_on(u3_noun bus, u3_noun fol);

    /* u3n_find(): return prog for given formula,
     *             split by key (u3_nul for none). RETAIN.
     */
      u3p(u3n_prog)
      u3n_find(u3_noun key, u3_noun fol);

    /* u3n_burn(): execute u3n_prog with bus as subject.
     */
      u3_noun
      u3n_burn(u3p(u3n_prog) pog_p, u3_noun bus);

    /* u3n_slam_on(): produce (gat sam).
    */
      u3_noun
      u3n_slam_on(u3_noun gat, u3_noun sam);

    /* u3n_kick_on(): fire `gat` without changing the sample.
    */
      u3_noun
      u3n_kick_on(u3_noun gat);

    /* u3n_nock_et(): produce .*(bus fol), as ++toon, in namespace.
    */
      u3_noun
      u3n_nock_et(u3_noun gul, u3_noun bus, u3_noun fol);

    /* u3n_slam_et(): produce (gat sam), as ++toon, in namespace.
    */
      u3_noun
      u3n_slam_et(u3_noun gul, u3_noun gat, u3_noun sam);

    /* u3n_nock_an(): as slam_in(), but with empty fly.
    */
      u3_noun
      u3n_nock_an(u3_noun bus, u3_noun fol);

    /* u3n_reap(): promote bytecode state.
     */
      void
      u3n_reap(u3p(u3h_root) har_p);

    /* u3n_reap_direct(): promote state of bytecode with direct calls
    */
      void
      u3n_reap_direct(u3p(u3h_root) dar_p);

    /* u3n_take(): copy junior bytecode state.
     */
      u3p(u3h_root)
      u3n_take(u3p(u3h_root) har_p);

    /* u3n_mark(): mark bytecode cache.
     */
      u3m_quac*
      u3n_mark();

    /* u3n_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_reclaim(void);

    /* u3n_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_rewrite_compact(void);

    /* u3n_free(): free bytecode cache.
     */
      void
      u3n_free(void);

    /* u3n_free_table(): free bytecode table
     */
      void
      u3n_free_table(u3p(u3h_root) har_p);

    /* u3n_ream(): refresh after restoring from checkpoint.
    */
      void
      u3n_ream(void);

    /* u3n_build_direct(): Compile [sub fol] pair with direct calls and
    *  its callees recursively
    */
      u3n_prog*
      u3n_build_direct(u3_noun sub,
        u3_noun fol,
        u3_noun cole,
        u3_noun code,
        u3_noun fols);

        u3n_prog*
        u3n_look_direct(u3_noun sub, u3_noun fol);

#endif /* ifndef U3_NOCK_H */
