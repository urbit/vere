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
          //  home-road page records and allocation
          //
          c3_d fre_cln_d;    //  frees that dirtied a clean page via its
                             //  page record
          c3_d alc_cln_d;    //  clean pages first written by a chunk
                             //  allocation
          c3_d alc_alt_d;    //    while a dirty page in the class had room
          c3_d alc_reu_d;    //  pages dirtied by a free, then allocated into
          //  u3m_reclaim() calls, and what they clear
          //
          c3_d rec_d;        //  reclaims
          c3_d rcb_d;        //  bytecode entries cleared
          c3_d rch_d;        //  hank entries cleared

          //  deferred gains across the state swap (u3a_defer_open())
          //
          c3_d dfw_d;        //  windows
          c3_d dfg_d;        //  gains deferred
          c3_d dfn_d;        //    on distinct nouns (summed over windows)
          c3_d dfx_d;        //  most nouns pending in one window
          c3_d dfc_d;        //  gains cancelled by a lose
          c3_d dfs_d;        //  gains written directly: table at its cap
          c3_d dfr_d;        //  gains written at close
          c3_d dfe_d;        //    on distinct nouns
          c3_d dfk_d;        //  table shrinks after a large window

          //  alignment of each new arvo state with the old, per
          //  u3e_measure_align(): home nouns referenced from the new
          //  state's cells on the event's road (which the take gains)
          //
          c3_d aln_all_d;    //  references
          c3_d aln_pos_d;    //    the same noun at the same position in
                             //    the old state
          c3_d aln_exc_d;    //    of which every old cell on the way has
                             //    a refcount of 1

          //  home-road hashcons set (off-loom)
          //
          c3_d hca_d;        //  members added, outside rebuilds
          c3_d hcd_d;        //  members removed as they were freed
          c3_d hcr_d;        //  rebuilds
          c3_d hcm_d;        //  rebuild time, ms
          c3_d hcc_d[7];     //  last rebuild: cells added per root,
                             //  per for byc war bas hot cod
          c3_d hcf_d[2];     //  last rebuild: page faults, major, minor
          c3_d hcp_d[3];     //  probes outside rebuilds: road-exit take,
                             //  older home nouns met in it, u3m_dedup()
          c3_d hch_d[4];     //  hits (an equal member elsewhere), by the
                             //  member's origin: added since the last
                             //  rebuild, caches (per for byc), warm jet
                             //  state (war bas hot), cold state alone

          //  bytecode programs
          //
          c3_d bcc_d;        //  compiled, on any road
          c3_d bca_d;        //  added to the home road's table
        } u3e_meter;

      extern volatile c3_y u3e_Phase;
      extern u3e_meter    u3e_Meter;

    /* u3e_measuring(): measurement is on.
    */
      c3_t
      u3e_measuring(void);

    /* u3e_measure_init(): start measuring if U3_MEASURE is set. the
    **                     header records [mod_c] ("live" or "replay")
    **                     and the snapshot interval: [sas_d] seconds
    **                     (live) or [sae_d] events (replay), 0 if none.
    */
      void
      u3e_measure_init(const c3_c* dir_c,
                       const c3_c* ver_c,
                       const c3_c* who_c,
                       const c3_c* mod_c,
                       c3_d        sas_d,
                       c3_d        sae_d);

    /* u3e_measure_ms(): wall-clock milliseconds, to time an operation
    **                   for u3e_measure_op() or u3e_measure_note().
    */
      c3_d
      u3e_measure_ms(void);

    /* u3e_measure_op(): if measuring, log a pack, meld or melt [op_c],
    **                   run because of [why_c], with melt options
    **                   [opt_w], begun at [bef_d] (u3e_measure_ms())
    **                   and freeing [gan_w] words.
    */
      void
      u3e_measure_op(const c3_c* op_c,
                     const c3_c* why_c,
                     c3_w        opt_w,
                     c3_d        bef_d,
                     c3_w        gan_w);

    /* u3e_measure_dir(): the pier at [dir_c] keeps measurement logs.
    */
      c3_t
      u3e_measure_dir(const c3_c* dir_c);

    /* u3e_measure_note(): log an offline [op_c] (as u3e_measure_op())
    **                     in a log of its own, if the pier at [dir_c]
    **                     keeps measurement logs, whether or not
    **                     U3_MEASURE is set.
    */
      void
      u3e_measure_note(const c3_c* dir_c,
                       const c3_c* ver_c,
                       const c3_c* who_c,
                       const c3_c* op_c,
                       c3_w        opt_w,
                       c3_d        bef_d,
                       c3_w        gan_w);

    /* u3e_measure_event(): record pages changed by the task just
    **                      completed. call on the home road.
    */
      void
      u3e_measure_event(c3_d eve_d, const c3_c* tag_c);

    /* u3e_measure_align(): at the end of a poke, on the event's road,
    **                      compare the new arvo state [new] with the
    **                      old [old] by position. read-only.
    */
      void
      u3e_measure_align(u3_noun new, u3_noun old);

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

    /* allocator hooks: call only if u3e_Measuring, on the home road.
    */
      extern c3_t u3e_Measuring;  //  U3_MEASURE is on

      /* u3e_measure_dirty(): page [pag_w] is dirty since the last save,
      **                      or past the image (new).
      */
        c3_t
        u3e_measure_dirty(c3_w pag_w);

      /* u3e_measure_free(): a free will write the page record on [pag_w].
      */
        void
        u3e_measure_free(c3_w pag_w);

      /* u3e_measure_alloc(): a chunk allocation will write [pag_w];
      **                      [alt_t] if a dirty page in its class has room.
      */
        void
        u3e_measure_alloc(c3_w pag_w, c3_t alt_t);

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
