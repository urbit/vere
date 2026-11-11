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
  *** function of the parts of the subject it uses.  Each function is
  *** compiled here from the core's IR to bytecode for a register
  *** machine whose registers are slots on the road stack.
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
      u3_noun          bell;          //  [sock formula] of the callee
      u3_noun          ring;          //  ~ or [path axis] of its jet
      u3p(struct _u3nc_prog) pog_p;   //  callee program, or 0 until linked
      c3_h             sot_h;         //  first argument slot (offset in sot_u)
      c3_h             len_h;         //  number of argument slots
      c3_h             cid_h;         //  memo cache for a memoized call
      c3_h             des_h;         //  slot for the product; none in tail position
      c3_o             mon_o;         //  call by subject: the one argument is
                                      //  the whole subject, pog_p its unary program
      c3_o             ice_o;         //  compiled trusting the jet arm: its
                                      //  array driver runs unchecked (CAF);
                                      //  else under the flags of the arm
                                      //  (CAC).  For u3nc_stale()
      u3j_harm*        ham_u;         //  jet arm in the hot state: its flags
      const u3u_harm*  arm_u;         //  its array driver, or NULL
    } u3nc_dire;

  /* u3nc_prog: program compiled from the IR of one function.
  */
    typedef struct _u3nc_prog {
      c3_h       tot_h;               //  number of slots
      c3_h       arg_h;               //  number of arguments (first slots)
      u3_noun    ned;                 //  need-ordered shape of the arguments
      u3_noun    jet;                 //  (list ring): the jets the ops that
                                      //  stand in for calls (DEC, ADD) trust,
                                      //  for u3nc_stale()
      struct {
        c3_h       len_h;             //  length of bytecode (bytes)
        c3_y*      ops_y;             //  bytecode
      } byc_u;
      struct {
        c3_h       len_h;             //  number of literals
        u3_noun*   non;               //  literals
      } lit_u;
      struct {
        c3_h       len_h;             //  number of call sites
        u3nc_dire* dat_u;             //  call sites
      } dir_u;
      struct {
        c3_h       len_h;             //  number of argument slots
        c3_h*      sot_h;             //  argument slots of all call sites
        c3_y*      kon_y;             //  whether each is consumed by its call
      } sot_u;
#ifdef U3NC_STAT
      c3_d       ent_d;               //  activations (u3nc_hot())
      c3_d       con_d;               //  cells made by its CONs (u3nc_hot())
      c3_d       bor_d;               //  drop-only parameters, bit per argument
      c3_y*      ali_y;               //  by bytecode offset past an op: a
                                      //  borrowing HED/TAL that could alias
#endif
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
      c3_d jet_c;   //    TSC cycles in jets, the ones that punt too
      c3_d nok_d;   //  indirect calls (NOK) (U3NC_STAT)
      c3_d nok_c;   //    TSC cycles in _nc_entry() for them
      c3_d mat_d;   //  entries u3d_match() scanned in entry lookups
      c3_d mem_d;   //  memo lookups of memoized calls (CAM, CSM)
      c3_d meh_d;   //    of them hits
      c3_d mem_c;   //    TSC cycles making their keys and looking up
      c3_d mes_d;   //  memo saves of memoized calls' products
      c3_d mes_c;   //    TSC cycles in them
      c3_d hug_d;   //  nodes of socks walked by u3d_match() (direct.c)
      c3_d hus_d;   //    known subtrees compared with u3r_sing(), not
                    //    the same noun on both sides
      c3_d hus_c;   //    TSC cycles in those comparisons
      c3_d con_d;   //  cells made by CON
      c3_d uni_d;   //  cells freed by a consuming HED or TAL: the only
                    //  reference to the cell was the slot's
      c3_d dro_d;   //  indirect nouns dropped by DRO
      c3_d byc_d;   //  bytes of bytecode compiled
      c3_d lit_d;   //  literal table entries compiled
      c3_d imm_d;   //  IR imm ops of a direct atom encoded, of which:
      c3_d imz_d;   //    of 0
      c3_d imo_d;   //    of 1
      c3_d imw_d;   //    to a slot of two bytes or more
      c3_d kil_d;   //  kil ops encoded (a build that doesn't zero slots)
      c3_d dre_d;   //  DRO ops encoded (U3NC_STAT)
      c3_d drb_d;   //  bytes of them (U3NC_STAT)
      c3_d edb_d;   //  blocks of drops made for an edge
      c3_d bld_d;   //  microseconds in _nc_build() (U3NC_STAT)
      c3_d iml_d;   //  IR iml ops (literal loads) encoded
      c3_d tra_d;   //  calls of transferring array jets (CAX, JMX)
      c3_d mut_d;   //  arguments a transferring jet edited in place
      //  reuse and borrowing opportunity counters (U3NC_STAT)
      c3_d dek_d;   //  consuming HED/TAL executed
      c3_d deb_d;   //  borrowing HED/TAL executed
      c3_d tok_d;   //  HER/TAR executed: consuming HED/TAL paired with a COR
      c3_d rus_d;   //  cells reused by COR
      c3_d spa_d;   //  CONs that find a spare cell freed by an unpaired
                    //  HED/TAL earlier in the same activation
      c3_d spg_d;   //  CONs that find such a spare cell, global (any activation)
      c3_d spw_d;   //  unpaired unique frees wasted: a global spare was held
      c3_d drt_d;   //  DROs executed
      c3_d kal_d;   //  arguments moved into callees (CAL, CAM, JMP, JMS)
      c3_d gan_d;   //  of them, gained (the caller keeps its reference)
      c3_d bor_d;   //  of the gained, to a drop-only parameter: borrowing
                    //  would save the gain and the callee's drop
      c3_d bok_d;   //  consumed, to a drop-only parameter of a call
      c3_d bot_d;   //  consumed, to a drop-only parameter of a tail call
      c3_d dbi_d;   //  borrowing HED/TAL whose product is an indirect noun
      c3_d ali_d;   //  borrowing HED/TAL whose product is only read and
                    //  dropped in its block while the source stays live:
                    //  an alias, needing no gain and no drop
      c3_d alx_d;   //  of them, with an indirect product
    } u3nc_stat;

    extern u3nc_stat u3nc_Stat;

  /* u3nc_stat(): count an event, in a build with the runtime counters.
  */
#ifdef U3NC_STAT
#  define u3nc_stat(fel)  (u3nc_Stat.fel++)
#else
#  define u3nc_stat(fel)  ((void)0)
#endif

  /**  Functions.
  **/
    /* u3nc_nock_on(): produce .*(bus fol).
    */
      u3_noun
      u3nc_nock_on(u3_noun bus, u3_noun fol);

    /* u3nc_slam_on(): produce (gat sam), compiled where it can be.
    */
      u3_noun
      u3nc_slam_on(u3_noun gat, u3_noun sam);

    /* u3nc_able(): can this road run compiled nock: an SKA core is
    **              installed, the road isn't the virtualization of the
    **              +mink jet, whose trace goes to Hoon's +mook, and it isn't
    **              in a poke of the SKA core made from inside another one
    **              (u3a_ska_depth()), where the core would compile its own
    **              code again; nor in any poke of it without u3nc_Self.
    */
      c3_o
      u3nc_able(void);

    /* u3nc_Self: whether the SKA core runs compiled, compiling its own
    **            code on its first pokes (the default), or in u3n, for the
    **            quick tests of the harness.
    */
      extern c3_t u3nc_Self;

    /* u3nc_Gop: the unary program of the callee of the call by subject
    **           whose memoizing jet is being kicked (see _nc_memo_jet()),
    **           linked once by the site, for the driver's misses, or 0.
    */
      extern struct _u3nc_prog* u3nc_Gop;

    /* u3nc_gop_nock(): .*(bus fol) on a miss of a memoizing driver of
    **                  compiled nock: the program the site published, without
    **                  an entry search, if there is one, else u3nc_nock_on().
    **                  TRANSFERS.
    */
      u3_noun
      u3nc_gop_nock(u3_noun bus, u3_noun fol);

    /* u3nc_mean(): the product of the arm of a deferred %mean trap
    **              [%meaf bell arg] on the trace stack, as ++toon: the
    **              n-ary program of bell on the arguments, in a
    **              virtualization context without a namespace.  RETAINS.
    */
      u3_noun
      u3nc_mean(u3_noun bell, u3_noun arg);

    /* u3nc_scan(): analyze and compile [bus fol] without running it.
    */
      void
      u3nc_scan(u3_noun bus, u3_noun fol);

#ifdef U3NC_STAT
    /* u3nc_hot(): print the programs that made the most cells, num_w of
    **             them, dum_w of those in full.
    */
      void
      u3nc_hot(c3_w num_w, c3_w dum_w);
#endif

    /* u3nc_regs(): print a histogram of the argument counts of the n-ary
    **              programs of this road and its seniors.
    */
      void
      u3nc_regs(void);

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

    /* u3nc_drop(): free the programs of functions on the home road, lis a
    **              (list bell) that holds the callers of all of them, so no
    **              program is left linked to a dropped one.  RETAINS.
    */
      void
      u3nc_drop(u3_noun lis);

    /* u3nc_stale(): the rings of the total array jets whose trust (the ice
    **               flag of their arm) changed since the programs on the
    **               home road compiled calls through them: trusting a
    **               driver that is checked now, or the other way around.
    **               For u3d_hot(), which drops those programs.  Produces
    **               a (list ring).
    */
      u3_noun
      u3nc_stale(void);

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

    /* u3nc_link_deferred(): relink programs from the deferred worklist
    */
      void
      u3nc_link_deferred(u3p(u3h_root) def_p);

#endif /* ifndef U3_NOCK_COMPILE_H */
