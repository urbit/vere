/// @file

#ifndef U3_EVENTS_H
#define U3_EVENTS_H

#include "c3/c3.h"
#include "allocate.h"
#include "version.h"

  /** Data structures.
  **/
    /* u3e_line: control line.
    */
      typedef struct _u3e_line {
        c3_w pag_w;
        c3_h has_h;
      } u3e_line;

    /* u3e_control: memory change, control file.
    */
      typedef struct _u3e_control {
        u3e_version ver_h;                  //  version number
        c3_h        has_h;                  //  control checksum
        c3_w        tot_w;                  //  new page count
        c3_w        pgs_w;                  //  number of changed pages
        u3e_line    mem_u[];                //  per page
      } u3e_control;

    /* u3_cs_patch: memory change, top level.
    */
      typedef struct _u3_cs_patch {
        c3_i         ctl_i;
        c3_i         mem_i;
        c3_w         sip_w;
        u3e_control* con_u;
      } u3_ce_patch;

    /* u3e_image: memory segment, open file.
    */
      typedef struct _u3e_image {
        c3_c* nam_c;                        //  segment name
        c3_i  fid_i;                        //  open file, or 0
        c3_w  pgs_w;                        //  length in pages
      } u3e_image;

    /* u3e_pool: entire memory system.
    */
      typedef struct _u3e_pool {
        c3_c*     dir_c;                     //  path to
        c3_i      eph_i;                     //  ephemeral file descriptor
        c3_d*     dit_d;                     //  touched since last save
        c3_w      pag_w;                     //  number of pages (<= u3a_pages)
        c3_w      gar_w;                     //  guard page
        u3e_image img_u;                     //  image
      } u3e_pool;

    /* u3e_flaw: loom fault result.
    */
      typedef enum {
        u3e_flaw_sham = 0,                  //  bogus state
        u3e_flaw_base = 1,                  //  vm fail (mprotect)
        u3e_flaw_meme = 2,                  //  bail:meme
        u3e_flaw_good = 3                   //  handled
      } u3e_flaw;

  /** Measurement (jb/measure-base).
  **
  **  Hot-path counters (kicks, formula and battery identity) are
  **  added on top of this in jb/measure.
  **
  **  Enabled at runtime with U3_MEASURE=1 in the environment of the
  **  serf process; see u3e_measure_init(). Per-task page hashing
  **  (E lines) is off unless U3_MEASURE_HASH=1. When disabled, the only
  **  cost is the phase-tag stores and a few counter increments.
  **/
      /* u3e_phase: what the runtime is doing, for attributing
      ** first writes to snapshot pages. keep in sync with
      ** _ce_pha_names[] in events.c.
      */
        typedef enum {
          u3e_pha_none      =  0,  //  unattributed
          u3e_pha_cue       =  1,  //  decoding an ipc message
          u3e_pha_poke      =  2,  //  computing an event (senior writes)
          u3e_pha_peek      =  3,  //  computing a scry
          u3e_pha_road      =  4,  //  road entry/exit bookkeeping
          u3e_pha_take_pro  =  5,  //  u3m_love: product
          u3e_pha_take_jet  =  6,  //  u3m_love: jet state
          u3e_pha_take_byc  =  7,  //  u3m_love: bytecode
          u3e_pha_take_per  =  8,  //  u3m_love: persistent memo
          u3e_pha_take_for  =  9,  //  u3m_love: ford memo
          u3e_pha_take_dup  = 10,  //  u3m_love: hash-consing table
          u3e_pha_ford_drop = 11,  //  u3m_love: stale ford cache freed
          u3e_pha_reap_jet  = 12,  //  u3m_love: merge jet state
          u3e_pha_reap_byc  = 13,  //  u3m_love: merge bytecode
          u3e_pha_reap_per  = 14,  //  u3m_love: merge persistent memo
          u3e_pha_reap_for  = 15,  //  u3m_love: merge ford memo
          u3e_pha_reap_dup  = 16,  //  u3m_love: merge hash-consing table
          u3e_pha_roc_lose  = 17,  //  releasing the old arvo state
          u3e_pha_roc_misc  = 18,  //  rest of the state swap
          u3e_pha_mug_roc   = 19,  //  mugging the new arvo state
          u3e_pha_feck      = 20,  //  _mars_sure_feck
          u3e_pha_etch      = 21,  //  event log entry (u3_disk_plan)
          u3e_pha_jam       = 22,  //  encoding effects/responses
          u3e_pha_io_free   = 23,  //  freeing job/effects after encoding
          u3e_pha_post      = 24,  //  _mars_post (trims, reclaim, gc)
          u3e_pha_save      = 25,  //  snapshot save
          u3e_pha_task      = 26,  //  other mars tasks
          u3e_pha_pack      = 27,  //  compaction (u3m_pack, meld)
          u3e_pha_rec_wish  = 28,  //  u3m_reclaim: wish cache (u3v)
          u3e_pha_rec_han   = 29,  //  u3m_reclaim: hank table (u3j)
          u3e_pha_rec_byc   = 30,  //  u3m_reclaim: bytecode (u3n)
          u3e_pha_rec_memo  = 31,  //  u3m_reclaim: transient memo cache
          u3e_pha_rec_dup   = 32,  //  u3m_reclaim: hash-consing prune
          u3e_pha_trim_per  = 33,  //  _mars_post: persistent memo trim
          u3e_pha_trim_for  = 34,  //  _mars_post: ford memo trim
          u3e_pha_max       = 35
        } u3e_phase;

      /* u3e_meter: counters reported at each snapshot.
      */
        typedef struct {
          //  road exits, per promoted table:
          //  [0] all exits, [1] exits into the home road
          //
          struct {
            c3_d exi_d[2];   //  exits
            c3_d nul_d[2];   //  exits with an empty junior table
            c3_d ent_d[2];   //  junior entries at exit
          } tab_u[8];        //  war cod han bas byc per for dup
          //  nock 9 kicks through call sites (jb/measure)
          //
          c3_d jet_d;        //  jet fired
          c3_d dec_d;        //  jet declined (punted)
          c3_d noj_d;        //  registered core, no jet at this axis
          c3_d unr_d;        //  unregistered core
          //  formula lookups (_n_find): [road][src][outcome]
          //  road: 0 home, 1 inner; src: 0 nock 2, 1 call site;
          //  outcomes: u3e_fnd_* (jb/measure)
          //
          c3_d fnd_d[2][2][7];
          //  call-site battery checks (_cj_site_lock): [road][outcome]
          //  outcomes: u3e_lok_* (jb/measure)
          //
          c3_d lok_d[2][5];
          //  u3m_reclaim() calls, and what they clear
          //
          c3_d rec_d;        //  reclaims
          c3_d rcb_d;        //  bytecode entries cleared
          c3_d rch_d;        //  hank entries cleared
          c3_d pru_d[2];     //  dedup prune: [0] entries before, [1] removed
          //  state swap (fused take/lose), per u3e_measure_swap()
          //  (jb/measure)
          //
          c3_d tak_cop_d;    //  nouns copied by the product's take
          c3_d tak_gan_d;    //  home nouns gained by the product's take
          c3_d tak_cln_d;    //    of which on a page clean before the gain
          c3_d tak_net_d;    //    of which netted to zero after roc-lose
          c3_d tak_npg_d;    //  clean pages whose gains all netted to zero
          //  page records and allocation (home road, jb/measure)
          //
          c3_d fre_cln_d;    //  frees that dirtied a clean page record page
          c3_d alc_cln_d;    //  chunk allocations that dirtied a clean page
          c3_d alc_alt_d;    //    while a dirty page in the class had room
          c3_d alc_reu_d;    //  pages dirtied by a free, then allocated into
        } u3e_meter;

      /* formula lookup outcomes. "copy" means the table holds an
      ** equal formula at a different address; "cur"/"sen" say whether
      ** the formula we looked up lives on the current road or is
      ** senior to it.
      */
#       define u3e_fnd_cur_ptr   0  //  hit, current road, same pointer
#       define u3e_fnd_cur_cur   1  //  hit, current road, copy (formula on road)
#       define u3e_fnd_cur_sen   2  //  hit, current road, copy (formula senior)
#       define u3e_fnd_anc_ptr   3  //  hit, ancestor road, same pointer
#       define u3e_fnd_anc_cur   4  //  hit, ancestor road, copy (formula on road)
#       define u3e_fnd_anc_sen   5  //  hit, ancestor road, copy (formula senior)
#       define u3e_fnd_miss      6  //  compiled anew

      /* call-site battery check outcomes.
      */
#       define u3e_lok_ptr       0  //  cached battery, same pointer
#       define u3e_lok_cur       1  //  equal battery, copy on current road
#       define u3e_lok_sen       2  //  equal battery, copy senior
#       define u3e_lok_diff      3  //  different battery, relocked
#       define u3e_lok_none      4  //  empty site, locked

      extern volatile c3_y u3e_Phase;
      extern u3e_meter    u3e_Meter;
      extern c3_t         u3e_Measuring;  //  U3_MEASURE is on

    /* hot-path hooks (jb/measure); call only if u3e_Measuring.
    */
      /* u3e_measure_dirty(): page [pag_w] is dirty since the last save,
      **                      or new (past the image).
      */
        c3_t
        u3e_measure_dirty(c3_w pag_w);

      /* u3e_measure_gain(): the product's take gains home-road [som].
      */
        void
        u3e_measure_gain(u3_noun som);

      /* u3e_measure_lose(): releasing the old state decrements [som].
      */
        void
        u3e_measure_lose(u3_noun som);

      /* u3e_measure_swap(): the state swap is done.
      */
        void
        u3e_measure_swap(void);

      /* u3e_measure_drop(): forget gains not followed by a state swap.
      */
        void
        u3e_measure_drop(void);

      /* u3e_measure_free(): a free will write the page record on [pag_w].
      */
        void
        u3e_measure_free(c3_w pag_w);

      /* u3e_measure_alloc(): a chunk allocation will write [pag_w];
      **                      [alt_t] if a dirty page in its class has room.
      */
        void
        u3e_measure_alloc(c3_w pag_w, c3_t alt_t);

    /* u3e_measure_init(): start measuring if U3_MEASURE is set.
    */
      void
      u3e_measure_init(const c3_c* dir_c);

    /* u3e_measure_event(): record pages changed by the task just
    **                      completed. call on the home road.
    */
      void
      u3e_measure_event(c3_d eve_d, const c3_c* tag_c);

  /** Globals.
  **/
      /// Snapshotting system.
      extern u3e_pool u3e_Pool;
#     define u3P u3e_Pool

  /** Constants.
  **/

  /** Functions.
  **/
    /* u3e_backup(): copy the snapshot from [pux_c] to [pax_c],
     * overwriting optional.
    */
      c3_o
      u3e_backup(c3_c* pux_c, c3_c* pax_c, c3_o ovw_o);

    /* u3e_fault(): handle a memory fault.
    */
      u3e_flaw
      u3e_fault(u3_post low_p, u3_post hig_p, u3_post off_p);

    /* u3e_save(): update the checkpoint.
    */
      void
      u3e_save(u3_post low_p, u3_post hig_p);

    /* u3e_toss(): discard ephemeral pages.
    */
      void
      u3e_toss(u3_post low_p, u3_post hig_p);

    /* u3e_live(): start the persistence system.  Return c3y if no image.
    */
      c3_o
      u3e_live(c3_o nuu_o, c3_c* dir_c);

    /* u3e_stop(): gracefully stop the persistence system.
    */
      void
      u3e_stop(void);

    /* u3e_yolo(): disable dirty page tracking, read/write whole loom.
    */
      c3_o
      u3e_yolo(void);

    /* u3e_foul(): dirty all the pages of the loom.
    */
      void
      u3e_foul(void);

    /* u3e_init(): initialize guard page tracking.
    */
      void
      u3e_init(void);

    /* u3e_ward(): reposition guard page if needed.
    */
      void
      u3e_ward(u3_post low_p, u3_post hig_p);

      c3_i
      u3e_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z, c3_i mod_i);

#endif /* ifndef U3_EVENTS_H */
