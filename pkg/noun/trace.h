/// @file

#ifndef U3_TRACE_H
#define U3_TRACE_H

#include <fcntl.h>
#include <unistd.h>

#include "c3/c3.h"
#include "options.h"
#include "types.h"
#include "allocate.h"

#ifdef U3_CPU_DEBUG
# include "options.h"
#endif

#ifdef U3_OS_osx
  #define SLOW_STACK_NAME  "/spin_stack_page_%d"
#else
  #define SLOW_STACK_NAME  "/spin_stack_page_%s_%d"
#endif
#define TRACE_PSIZE (((c3_w)1) << (u3a_page +2))

#ifdef U3_OS_osx
  #define STAT_PAGE_NAME  "/stat_page_%d"
#else
  #define STAT_PAGE_NAME  "/stat_page_%s_%d"
#endif
#define U3T_STAT_VERSION  1     //  u3t_stat layout version
#define U3T_STAT_PATS     16    //  snapshot patch history length
#define U3T_STAT_HITS     100   //  disk batch histogram length

  /** Data structures.
  **/
    /* u3t_trace: fast execution flags.
    */
      typedef struct _u3t_trace {
        c3_o noc_o;                 //  now executing in nock interpreter
        c3_o glu_o;                 //  now executing in jet glue
        c3_o mal_o;                 //  now executing in allocator
        c3_o far_o;                 //  now executing in fragmentor
        c3_o coy_o;                 //  now executing in copy
        c3_o euq_o;                 //  now executing in equal
      } u3t_trace;

   /* u3t_spin: %spin hint stack
    */
    typedef struct {
      c3_h seq_h;  //  seqlock: odd while the serf is mid-write, else even
      c3_h off_h;
      c3_h fow_h;
      c3_y dat_y[TRACE_PSIZE - 3*sizeof(c3_h)];
    } u3t_spin;

   /* u3t_patch: snapshot patch record.
    */
    typedef struct {
      c3_d eve_d;  //  event number at snapshot
      c3_d tim_d;  //  wall-clock time, unix ms
      c3_d pgs_d;  //  dirty pages written
      c3_d byt_d;  //  bytes written
      c3_d sip_d;  //  free pages skipped
      c3_d img_d;  //  image length after apply, pages
      c3_d wit_d;  //  write and sync patch, us
      c3_d app_d;  //  apply patch and sync image, us
    } u3t_patch;

   /* u3t_stat: runtime stats, written by the serf, read by the king.
    */
    typedef struct {
      c3_h seq_h;                       //  seqlock: odd while mid-write
      c3_h ver_h;                       //  U3T_STAT_VERSION
      c3_d tim_d;                       //  last update, unix ms
      struct {                          //  events
        c3_d sen_d;                     //    last requested
        c3_d dun_d;                     //    last processed
        c3_d sav_d;                     //    last snapshot
        c3_h mug_h;                     //    state hash
      } eve_u;
      struct {                          //  event log
        c3_o liv_o;                     //    live
        c3_d sen_d;                     //    commit requested
        c3_d dun_d;                     //    committed
        c3_d epo_d;                     //    current epoch
        c3_d put_d[2];                  //    pending save range, 0 if none
        c3_h hit_h[U3T_STAT_HITS];      //    batch histogram
      } dis_u;
      struct {                          //  memory, bytes unless noted
        c3_d pag_d;                     //    page size
        c3_d lom_d;                     //    loom size
        c3_d ful_d;                     //    home road
        c3_d hep_d;                     //    heap
        c3_d tmp_d;                     //    stack
        c3_d opn_d;                     //    open (free between heap/stack)
        c3_d img_d;                     //    snapshot image
        c3_d per_d;                     //    persistent cache, entries
        c3_d pem_d;                     //    persistent cache, max entries
        c3_d for_d;                     //    formula cache, entries
      } mem_u;
      struct {                          //  snapshot patches
        c3_d num_d;                     //    total patches written
        c3_d byt_d;                     //    total bytes written
        c3_d max_d;                     //    largest patch, bytes
        c3_h nex_h;                     //    next ring slot
        u3t_patch pat_u[U3T_STAT_PATS]; //    recent patches (ring)
      } pat_u;
    } u3t_stat;

  /**  Macros.
  **/
#   ifdef U3_CPU_DEBUG
#     define u3t_on(var)  \
        (u3T.var = (u3C.wag_h & u3o_debug_cpu) \
                 ? (c3n == u3T.var) ? c3y : (abort(), 0) \
                 : u3T.var)
#   else
#     define u3t_on(var)
#endif

#   ifdef U3_CPU_DEBUG
#     define u3t_off(var) \
        (u3T.var = (u3C.wag_h & u3o_debug_cpu) \
                 ? (c3y == u3T.var) ? c3n : (abort(), 0) \
                 : u3T.var)
#   else
#     define u3t_off(var)
#endif


  /**  Functions.
  **/
    /* u3t_init(): initialize tracing layer.
    */
      void
      u3t_init(void);

      /// @return  Number of entries written to the JSON trace file.
      c3_w
      u3t_trace_cnt(void);

      /// @return  Number of times u3t_trace_close() has been called.
      c3_w
      u3t_file_cnt(void);

    /* u3t_push(): push on trace stack.
    */
      void
      u3t_push(u3_noun mon);

    /* u3t_mean(): push `[%mean roc]` on trace stack.
    */
      void
      u3t_mean(u3_noun roc);

    /* u3t_drop(): drop from meaning stack.
    */
      void
      u3t_drop(void);

    /* u3t_slog(): print directly.
    */
      void
      u3t_slog(u3_noun hod);

    /* u3t_heck(): profile point.
    */
      void
      u3t_heck(u3_atom cog);

    /* u3t_samp(): sample.
    */
      void
      u3t_samp(void);

    /* u3t_come(): push on profile stack; return yes if active push.  RETAIN.
    */
      c3_o
      u3t_come(u3_noun bat);

    /* u3t_flee(): pop off profile stack.
    */
      void
      u3t_flee(void);

    /* u3t_trace_open(): opens the path for writing tracing information.
    */
      void
      u3t_trace_open(const c3_c *dir_c);

    /* u3t_trace_close(): closes the trace file. optional.
    */
      void
      u3t_trace_close(void);

    /* u3t_trace_time(): returns current time since system epoc,
     * whatever it is per system, in microseconds.
    */
      c3_d
      u3t_trace_time(void);

    /* u3t_nock_trace_push(): pushes a frame onto the trace stack;
    *  return yes if active push.
    */
      c3_o
      u3t_nock_trace_push(u3_noun lab);

    /* u3t_nock_trace_pop(): pop off trace stack.
    */
      void
      u3t_nock_trace_pop(void);

    /* u3t_event_trace(): record a lifecycle event.
    */
      void
      u3t_event_trace(const c3_c* name, c3_c type);

    /* u3t_damp(): print and clear profile data.
    */
      void
      u3t_damp(FILE* fil_u);

    /* u3t_boff(): turn profile sampling off.
    */
      void
      u3t_boff(void);

    /* u3t_boot(): turn sampling on.
    */
      void
      u3t_boot(void);

    /* u3t_slog_cap(): slog a tank with a caption with
    ** a given priority c3_h (assumed 0-3).
    */
      void
      u3t_slog_cap(c3_h pri_l, u3_noun cap, u3_noun tan);

    /* u3t_slog_trace(): given a c3_h priority pri and a raw stack tax
    ** flop the order into start-to-end, render, and slog each item
    ** until done.
    */
      void
      u3t_slog_trace(c3_h pri_l, u3_noun tax);

    /* u3t_slog_nara(): slog only the deepest road's trace with
    ** c3_h priority pri
    */
      void
      u3t_slog_nara(c3_h pri_l);

    /* u3t_slog_hela(): join all roads' traces together into one tax
    ** and pass it to slog_trace along with the given c3_h priority pri_l
    */
      void
      u3t_slog_hela(c3_h pri_l);

    /* u3t_etch_meme(): report memory stats at call time
    */
      u3_noun
      u3t_etch_meme(c3_w mod_w);

    /* u3t_sstack_init: initalize a root node on the spin stack
     */
      void
      u3t_sstack_init(c3_d* who_d);

    /* u3t_sstack_init: initalize a root node on the spin stack 
     */
      u3t_spin*
      u3t_sstack_open(c3_d* who_d);

    /* u3t_sstack_exit: tear down the spin stack (serf side).
     */
      void
      u3t_sstack_exit(void);

    /* u3t_sstack_close: tear down a spin stack mapping (king side).
     */
      void
      u3t_sstack_close(u3t_spin* stk_u);

    /* u3t_sstack_push: push a noun on the spin stack.
     */
      void
      u3t_sstack_push(u3_noun nam);

    /* u3t_sstack_pop: pop a noun from the spin stack.
     */
      void
      u3t_sstack_pop(void);

    /* u3t_stat_init: create the stats page (serf side).
     */
      void
      u3t_stat_init(c3_d* who_d);

    /* u3t_stat_open: map the stats page read-only (king side).
     */
      u3t_stat*
      u3t_stat_open(c3_d* who_d);

    /* u3t_stat_exit: tear down the stats page (serf side).
     */
      void
      u3t_stat_exit(void);

    /* u3t_stat_close: tear down a stats page mapping (king side).
     */
      void
      u3t_stat_close(u3t_stat* sat_u);

    /* u3t_stat_writ_beg: begin a stats update; NULL if no page.
    **                    must be paired with u3t_stat_writ_end().
     */
      u3t_stat*
      u3t_stat_writ_beg(void);

    /* u3t_stat_writ_end: finish a stats update.
     */
      void
      u3t_stat_writ_end(void);

    /* u3t_stat_read: consistent copy of [sat_u] into [out_u].
     */
      c3_o
      u3t_stat_read(const u3t_stat* sat_u, u3t_stat* out_u);

    /* u3t_stat_patch: record a snapshot patch.
     */
      void
      u3t_stat_patch(const u3t_patch* pat_u);


  /** Globals.
  **/
      /// Tracing profiler.
      extern u3t_trace u3t_Trace;
      extern u3t_spin* u3t_Spin;
      extern u3t_stat* u3t_Stat;

#     define u3T u3t_Trace


#endif /* ifndef U3_TRACE_H */
