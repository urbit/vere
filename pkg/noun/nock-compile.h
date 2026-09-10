/// @file

#ifndef U3_NOCK_COMPILE_H
#define U3_NOCK_COMPILE_H

#include "c3/c3.h"
#include "jets.h"
#include "types.h"
#include "zave.h"

  /** u3nc: nock compiled with subject knowledge analysis.
  ***
  *** The SKA core (direct.c) turns a [subject formula] pair into a
  *** function of the parts of the subject it uses, and its callees
  *** into the same.  Each function is compiled here from the core's
  *** IR to bytecode for a register machine whose registers are slots
  *** on the road stack.
  ***
  *** Programs live in two tables of the road:
  ***
  ***   ska.dir_p: bell [sock formula] -> program of the function,
  ***              called directly by other programs with its arguments
  ***              in the slots it asks for;
  ***   ska.ent_p: formula -> (list [sock program]), the entry programs,
  ***              functions of the whole subject as their one argument.
  **/

  /* u3nc_dire: a call site.
  */
    struct _u3nc_prog;
    typedef struct {
      u3_noun          bell;   //  [sock formula] of the callee
      u3_noun          ring;   //  ~ or [path axis] of its jet
      u3p(struct _u3nc_prog) pog_p;  //  callee program, or 0 until linked
      c3_w             sot_w;  //  first argument slot (offset in sot_u)
      c3_w             len_w;  //  number of argument slots
      c3_w             cid_w;  //  memo cache (u3z_cid) for a memoized call
      c3_o             dir_o;  //  direct call: link pog_p by bell
      u3j_harm*        ham_u;  //  jet arm taking the core, nullable
      const u3u_harm*  arm_u;  //  jet arm taking the arguments, nullable
    } u3nc_dire;

  /* u3nc_prog: program compiled from the IR of one function.
  */
    typedef struct _u3nc_prog {
      c3_w       tot_w;               //  number of slots
      c3_w       arg_w;               //  number of arguments (first slots)
      u3_noun    ned;                 //  need-ordered shape of the arguments
      struct {
        c3_w       len_w;             //  length of bytecode (bytes)
        c3_y*      ops_y;             //  bytecode
      } byc_u;
      struct {
        c3_w       len_w;             //  number of literals
        u3_noun*   non;               //  literals
      } lit_u;
      struct {
        c3_w       len_w;             //  number of call sites
        u3nc_dire* dat_u;             //  call sites
      } dir_u;
      struct {
        c3_w       len_w;             //  number of argument slots
        c3_w*      sot_w;             //  argument slots of all call sites
      } sot_u;
    } u3nc_prog;

  /* u3nc_stat: cumulative counters; the runtime ones need U3NC_STAT.
  */
    typedef struct {
      c3_d ent_d;   //  entry lookups
      c3_d com_d;   //  compiled programs
      c3_d arm_d;   //  compiled call sites with a jet driver
      c3_d rin_d;   //  compiled call sites with a ring but no driver
      c3_d dir_d;   //  direct calls
      c3_d sub_d;   //  calls by subject
      c3_d jet_d;   //  jet hits
    } u3nc_stat;

    extern u3nc_stat u3nc_Stat;

  /**  Functions.
  **/
    /* u3nc_nock_on(): produce .*(bus fol).
    */
      u3_noun
      u3nc_nock_on(u3_noun bus, u3_noun fol);

    /* u3nc_take(): copy junior program tables; sets *dir_p and *ent_p
    **              to the copies.
    */
      void
      u3nc_take(u3p(u3h_root)* dir_p, u3p(u3h_root)* ent_p);

    /* u3nc_reap(): promote taken program tables.
    */
      void
      u3nc_reap(u3p(u3h_root) dir_p, u3p(u3h_root) ent_p);

    /* u3nc_mark(): mark the SKA core and program tables for gc.
    */
      u3m_quac*
      u3nc_mark(void);

    /* u3nc_reclaim(): free the programs to reclaim memory.
    */
      void
      u3nc_reclaim(void);

    /* u3nc_rewrite_compact(): rewrite the SKA state for compaction.
    */
      void
      u3nc_rewrite_compact(void);

    /* u3nc_free(): free the programs and their tables.
    */
      void
      u3nc_free(void);

    /* u3nc_ream(): refresh after restoring from checkpoint.
    */
      void
      u3nc_ream(void);

#endif /* ifndef U3_NOCK_COMPILE_H */
