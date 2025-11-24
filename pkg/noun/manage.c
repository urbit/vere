/// @file

#include "manage.h"

#include <ctype.h>
#ifndef U3_OS_windows
#include <dlfcn.h>
#endif
#include <errno.h>
#include <signal.h>
#if defined(U3_OS_osx)
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#if defined(U3_OS_linux)
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include "allocate.h"
#include "backtrace.h"
#include "events.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "jets/k.h"
#include "jets/q.h"
#include "log.h"
#include "nock.h"
#include "openssl/crypto.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "urcrypt.h"
#include "vortex.h"
#include "whereami.h"
#include "xtract.h"

//  XX stack-overflow recovery should be gated by -a
//
#undef NO_OVERFLOW

      /* (u3_noun)setjmp(u3R->esc.buf): setjmp within road.
      */
#if 0
        c3_o
        u3m_trap(void);
#else
#       define u3m_trap() (u3_noun)(_setjmp(u3R->esc.buf))
#endif

      /* u3m_signal(): treat a nock-level exception as a signal interrupt.
      */
        void
        u3m_signal(u3_noun sig_l);

      /* u3m_dump(): dump the current road to stderr.
      */
        void
        u3m_dump(void);

      /* u3m_fall(): return to parent road.
      */
        void
        u3m_fall(void);

      /* u3m_leap(): in u3R, create a new road within the existing one.
      */
        void
        u3m_leap(c3_w pad_w);

      /* u3m_golf(): record cap length for u3m_flog().
      */
        c3_w
        u3m_golf(void);

      /* u3m_flog(): pop the cap.
      **
      **    A common sequence for inner allocation is:
      **
      **    c3_w gof_w = u3m_golf();
      **    u3m_leap();
      **    //  allocate some inner stuff...
      **    u3m_fall();
      **    //  inner stuff is still valid, but on cap
      **    u3m_flog(gof_w);
      **
      ** u3m_flog(0) simply clears the cap.
      */
        void
        u3m_flog(c3_w gof_w);

      /* u3m_soft_top(): top-level safety wrapper.
      */
        u3_noun
        u3m_soft_top(c3_w    mil_w,                     //  timer ms
                     c3_w    pad_w,                     //  base memory pad
                     u3_funk fun_f,
                     u3_noun   arg);


//  u3m_signal uses restricted functionality signals for compatibility reasons:
//  some platforms may not provide true POSIX asynchronous signals and their
//  compat layer will then implement this restricted functionality subset.
//  u3m_signal never needs to interrupt I/O operations, its signal handlers
//  do not manipulate signals, do not modify shared state, and always either
//  return or longjmp.
//
static rsignal_jmpbuf u3_Signal;

#ifndef U3_OS_windows
#include "sigsegv.h"

#ifndef SIGSTKSZ
# define SIGSTKSZ 16384
#endif
#ifndef NO_OVERFLOW
static uint8_t Sigstk[SIGSTKSZ];
#endif
#endif

#ifdef U3_OS_windows
#include "veh_handler.h"
#endif

#if 0
/* _cm_punt(): crudely print trace.
*/
static void
_cm_punt(u3_noun tax)
{
  u3_noun xat;

  for ( xat = tax; xat; xat = u3t(xat) ) {
    u3m_p("&", u3h(xat));
  }
}
#endif

/* _cm_emergency(): write emergency text to stderr, never failing.
*/
static void
_cm_emergency(c3_c* cap_c, c3_l sig_l)
{
  write(2, "\r\n", 2);
  write(2, cap_c, strlen(cap_c));

  if ( sig_l ) {
    write(2, ": ", 2);
    write(2, &sig_l, 4);
  }

  write(2, "\r\n", 2);
}

static void _cm_overflow(void *arg1, void *arg2, void *arg3)
{
  (void)(arg1);
  (void)(arg2);
  (void)(arg3);
  u3m_signal(c3__over);
}

/* _cm_signal_handle(): handle a signal in general.
*/
static void
_cm_signal_handle(c3_l sig_l)
{
#ifndef U3_OS_windows
  if ( c3__over == sig_l ) {
#ifndef NO_OVERFLOW
    sigsegv_leave_handler(_cm_overflow, NULL, NULL, NULL);
#endif
  } else
#endif
  {
    u3m_signal(sig_l);
  }
}

#ifndef NO_OVERFLOW
static void
#ifndef U3_OS_windows
_cm_signal_handle_over(int emergency, stackoverflow_context_t scp)
#else 
_cm_signal_handle_over(int x)
#endif
{
  _cm_signal_handle(c3__over);
}
#endif

static void
_cm_signal_handle_term(int x)
{
  //  Ignore if we are using base memory from work memory, very rare.
  //
  if ( (0 != u3H->rod_u.kid_p) && (&(u3H->rod_u) == u3R) ) {
    _cm_emergency("ignored", c3__term);
  }
  else {
    _cm_signal_handle(c3__term);
  }
}

static void
_cm_signal_handle_intr(int x)
{
  //  Interrupt: stop work.  Ignore if not working, or (rarely) using base.
  //
  if ( &(u3H->rod_u) == u3R ) {
    _cm_emergency("ignored", c3__intr);
  }
  else {
    _cm_signal_handle(c3__intr);
  }
}

static void
_cm_signal_handle_alrm(int x)
{
  _cm_signal_handle(c3__alrm);
}

/* _cm_signal_reset(): reset top road after signal longjmp.
*/
static void
_cm_signal_reset(void)
{
  u3R = &u3H->rod_u;
  u3R->cap_p = u3R->mat_p;
  u3R->ear_p = 0;
  u3R->kid_p = 0;
}

#if 0
/* _cm_stack_recover(): recover stack trace, with lacunae.
*/
static u3_noun
_cm_stack_recover(u3a_road* rod_u)
{
  c3_w len_w;

  len_w = 0;
  {
    u3_noun tax = rod_u->bug.tax;

    while ( tax ) {
      len_w++;
      tax = u3t(tax);
    }

    if ( len_w < 4096 ) {
      return u3a_take(rod_u->bug.tax);
    }
    else {
      u3_noun beg, fin;
      c3_w i_w;

      tax = rod_u->bug.tax;
      beg = u3_nul;
      for ( i_w = 0; i_w < 2048; i_w++ ) {
        beg = u3nc(u3a_take(u3h(tax)), beg);
        tax = u3t(tax);
      }
      beg = u3kb_flop(beg);

      for ( i_w = 0; i_w < (len_w - 4096); i_w++ ) {
        tax = u3t(tax);
      }
      fin = u3nc(u3nc(c3__lose, c3__over), u3a_take(tax));

      return u3kb_weld(beg, fin);
    }
  }
}
#endif

/* _cm_stack_unwind(): unwind to the top level, preserving all frames.
*/
static u3_noun
_cm_stack_unwind(void)
{
  u3_noun tax;

  while ( u3R != &(u3H->rod_u) ) {
    u3_noun yat = u3m_love(u3R->bug.tax);

    u3R->bug.tax = u3kb_weld(yat, u3R->bug.tax);
  }
  tax = u3R->bug.tax;

  u3R->bug.tax = 0;
  return tax;
}

/* _cm_signal_recover(): recover from a deep signal, after longjmp.  Free arg.
*/
static u3_noun
_cm_signal_recover(c3_l sig_l, u3_noun arg)
{
  u3_noun tax;

  //  Unlikely to be set, but it can be made to happen.
  //
  tax = u3H->rod_u.bug.tax;
  u3H->rod_u.bug.tax = 0;

  if ( NULL != stk_u ) {
    stk_u->off_w = u3H->rod_u.off_w;
    stk_u->fow_w = u3H->rod_u.fow_w;
  }

  if ( &(u3H->rod_u) == u3R ) {
    //  A top-level crash - rather odd.  We should GC.
    //
    _cm_emergency("recover: top", sig_l);
    u3C.wag_w |= u3o_check_corrupt;

    //  Reset the top road - the problem could be a fat cap.
    //
    _cm_signal_reset();

    if ( (c3__meme == sig_l) && (u3a_open(u3R) <= 256) ) {
      // Out of memory at the top level.  Error becomes c3__full,
      // and we release the emergency buffer.  To continue work,
      // we need to readjust the image, eg, migrate to 64 bit.
      //
      u3z(u3R->bug.mer);
      u3R->bug.mer = 0;
      sig_l = c3__full;
    }
    return u3nt(3, sig_l, tax);
  }
  else {
    u3_noun pro;

    //  A signal was generated while we were within Nock.
    //
    _cm_emergency("recover: dig", sig_l);

#if 0
    //  Descend to the innermost trace, collecting stack.
    //
    {
      u3a_road* rod_u;

      u3R = &(u3H->rod_u);
      rod_u = u3R;

      while ( rod_u->kid_p ) {
#if 0
        u3l_log("collecting %d frames",
              u3kb_lent((u3to(u3_road, rod_u->kid_p)->bug.tax));
#endif
        tax = u3kb_weld(_cm_stack_recover(u3to(u3_road, rod_u->kid_p)), tax);
        rod_u = u3to(u3_road, rod_u->kid_p);
      }
    }
#else
    tax = _cm_stack_unwind();
#endif
    pro = u3nt(3, sig_l, tax);
    _cm_signal_reset();

    u3z(arg);
    return pro;
  }
}

/* _cm_signal_deep(): start deep processing; set timer for [mil_w] or 0.
*/
static void
_cm_signal_deep(void)
{
  //  disable outer system signal handling
  //
  if ( 0 != u3C.sign_hold_f ) {
    u3C.sign_hold_f();
  }

#ifndef NO_OVERFLOW
#ifndef U3_OS_windows
  if ( 0 != stackoverflow_install_handler(_cm_signal_handle_over, Sigstk, SIGSTKSZ)) {
    u3l_log("unable to install stack overflow handler");
    abort();
  }
#else
  rsignal_install_handler(SIGSTK, _cm_signal_handle_over);
#endif
#endif
  rsignal_install_handler(SIGINT, _cm_signal_handle_intr);
  rsignal_install_handler(SIGTERM, _cm_signal_handle_term);
  rsignal_install_handler(SIGVTALRM, _cm_signal_handle_alrm);

  // Provide a little emergency memory, for use in case things
  // go utterly haywire.
  //
  if ( 0 == u3H->rod_u.bug.mer ) {
    u3H->rod_u.bug.mer = u3i_string(
      "emergency buffer with sufficient space to cons the trace and bail"
    );
  }

  u3t_boot();
}

/* _cm_signal_done():
*/
static void
_cm_signal_done(void)
{
  rsignal_deinstall_handler(SIGINT);
  rsignal_deinstall_handler(SIGTERM);
  rsignal_deinstall_handler(SIGVTALRM);

#ifndef NO_OVERFLOW
#ifndef U3_OS_windows
  stackoverflow_deinstall_handler();
#else
  rsignal_install_handler(SIGSTK, _cm_signal_handle_over);
#endif
#endif
  {
    struct itimerval itm_u;

    timerclear(&itm_u.it_interval);
    timerclear(&itm_u.it_value);

    if ( rsignal_setitimer(ITIMER_VIRTUAL, &itm_u, 0) ) {
      u3l_log("loom: clear timer failed %s", strerror(errno));
    }
  }

  //  restore outer system signal handling
  //
  if ( 0 != u3C.sign_move_f ) {
    u3C.sign_move_f();
  }

  u3t_boff();
}

/* u3m_signal(): treat a nock-level exception as a signal interrupt.
*/
void
u3m_signal(u3_noun sig_l)
{
  rsignal_longjmp(u3_Signal, sig_l);
}

/* u3m_file(): load file, as atom, or bail.
*/
u3_noun
u3m_file(c3_c* pas_c)
{
  struct stat buf_b;
  c3_i        fid_i = c3_open(pas_c, O_RDONLY, 0644);
  c3_w        fln_w, red_w;
  c3_y*       pad_y;

  if ( (fid_i < 0) || (fstat(fid_i, &buf_b) < 0) ) {
    u3l_log("%s: %s", pas_c, strerror(errno));
    return u3m_bail(c3__fail);
  }
  fln_w = buf_b.st_size;
  pad_y = c3_malloc(buf_b.st_size);

  red_w = read(fid_i, pad_y, fln_w);
  close(fid_i);

  if ( fln_w != red_w ) {
    c3_free(pad_y);
    return u3m_bail(c3__fail);
  }
  else {
    u3_noun pad = u3i_bytes(fln_w, (c3_y *)pad_y);
    c3_free(pad_y);

    return pad;
  }
}

/* u3m_mark(): mark all nouns in the road.
*/
u3m_quac**
u3m_mark(void)
{
  u3m_quac** qua_u = c3_malloc(sizeof(*qua_u) * 5);
  qua_u[0] = u3v_mark();
  qua_u[1] = u3j_mark();
  qua_u[2] = u3n_mark();
  qua_u[3] = u3a_mark_road();  // NB: must be the last thing marked
  qua_u[4] = NULL;

  return qua_u;
}

/* _pave_parts(): build internal tables.
*/
static void
_pave_parts(void)
{
  u3a_init_heap();

  if ( &(u3H->rod_u) != u3R ) {
    u3R->cel.cel_p = u3of(u3_post, u3a_walloc(1U << u3a_page));
  }

  u3R->cax.har_p = u3h_new_cache(u3C.hap_w);  //  transient
  u3R->cax.per_p = u3h_new_cache(u3C.per_w);  //  persistent
  u3R->jed.war_p = u3h_new();
  u3R->jed.cod_p = u3h_new();
  u3R->jed.han_p = u3h_new();
  u3R->jed.bas_p = u3h_new();
  u3R->byc.har_p = u3h_new();
  u3R->lop_p     = u3h_new();
  u3R->tim       = u3_nul;
  u3R->how.fag_w = 0;
}

static c3_d
_pave_params(void)
{
  //  pam_d bits:
  //  { word-size[1], virtual-bits[2], page-size[3], bytecode[5], ... }
  //
  //    word-size: 0==32, 1==64
  //    page-size: relative binary-log in bytes
  //
  //
  return 0
         ^ (u3a_vits << 1)
         ^ ((u3a_page + 2 - 12) << 3)
         ^ (U3N_VERLAT << 6);
}

/* _pave_home(): initialize pristine home road.
*/
static void
_pave_home(void)
{
  u3_post top_p = u3C.wor_i - u3a_walign;
  u3_post bot_p = 1U << u3a_page;

  u3H = u3to(u3v_home, 0);
  memset(u3H, 0, sizeof(u3v_home));
  u3H->ver_d = U3V_VERLAT;
  u3H->pam_d = _pave_params();

  u3R = &u3H->rod_u;

  u3R->rut_p = u3R->hat_p = bot_p;
  u3R->mat_p = u3R->cap_p = top_p;

  _pave_parts();
}

STATIC_ASSERT( (c3_wiseof(u3v_home) <= (1U << u3a_page)),
               "home road size" );

STATIC_ASSERT( ((c3_wiseof(u3v_home) * 4) == sizeof(u3v_home)),
               "home road alignment" );

STATIC_ASSERT( U3N_VERLAT < (1U << 5), "5-bit bytecode version" );

/* _find_home(): in restored image, point to home road.
*/
static void
_find_home(void)
{
  c3_d ver_d = *((c3_d*)u3_Loom);

  if ( ver_d != U3V_VERLAT ) {
    fprintf(stderr, "loom: checkpoint version mismatch: "
                    "have %" PRIu64 ", need %" PRIu64 "\r\n",
                    ver_d, U3V_VERLAT);
    abort();
  }

  c3_d pam_d = *((c3_d*)u3_Loom + 1);

  if ( pam_d & 1 ) {
    fprintf(stderr, "word-size mismatch: 64-bit snapshot in 32-bit binary\r\n");
    abort();
  }
  if ( ((pam_d >> 1) & 3) != u3a_vits ) {
    fprintf(stderr, "virtual-bits mismatch: %u in snapshot; %u in binary\r\n",
                    (c3_w)((pam_d >> 1) & 3), u3a_vits);
    abort();
  }
  if ( (12 + ((pam_d >> 3) & 7)) != (u3a_page + 2) ) {
    fprintf(stderr, "page-size mismatch: %u  in snapshot; %u in binary\r\n",
                    1U << (12 + ((pam_d >> 3) & 7)), (c3_w)u3a_page + 2);
    abort();
  }

  //  NB: the home road is always north
  //
  {
    u3_post top_p = u3C.wor_i - u3a_walign;

    u3H = u3to(u3v_home, 0);
    u3R = &u3H->rod_u;

    //  this looks risky, but there are no legitimate scenarios
    //  where it's wrong
    //
    u3R->mat_p = u3R->cap_p = top_p;
  }

  //  check for obvious corruption
  //
  {
    c3_w    nor_w;
    u3_post low_p, hig_p;
    u3m_water(&low_p, &hig_p);

    nor_w = (low_p + ((1 << u3a_page) - 1)) >> u3a_page;

    if ( nor_w > u3P.img_u.pgs_w ) {
      fprintf(stderr, "loom: corrupt size (%u, %u)\r\n",
                      nor_w, u3P.img_u.pgs_w);
      u3_assert(!"loom: corrupt size");
    }

    //  the north segment is in-order on disk; it being oversized
    //  doesn't necessarily indicate corruption.
    //
    if ( nor_w < u3P.img_u.pgs_w ) {
      fprintf(stderr, "loom: strange size north (%u, %u)\r\n",
                      nor_w, u3P.img_u.pgs_w);
    }

    //  XX move me
    //
    u3a_ream();
  }

  /* As a further guard against any sneaky loom corruption */
  u3a_loom_sane();

  _rod_vaal(u3R);

  if ( ((pam_d >> 6) & 31) != U3N_VERLAT ) {
    fprintf(stderr, "loom: discarding stale bytecode programs\r\n");
    u3j_ream();
    u3n_ream();
    u3n_reclaim();
    u3j_reclaim();
    u3H->pam_d = _pave_params();
  }

  //  if lop_p is zero than it is an old pier pre %loop hint, initialize the
  //  HAMT
  //
  if (!u3R->lop_p) {
    u3R->lop_p = u3h_new();
  }
}

/* u3m_pave(): instantiate or activate image.
*/
void
u3m_pave(c3_o nuu_o)
{
  if ( c3y == nuu_o ) {
    _pave_home();
  }
  else {
    _find_home();
  }
}

#if 0
/* u3m_clear(): clear all allocated data in road.
*/
void
u3m_clear(void)
{
  u3h_free(u3R->cax.har_p);
  u3j_free();
  u3n_free();
}

void
u3m_dump(void)
{
  c3_w hat_w;
  c3_w fre_w = 0;
  c3_w i_w;

  hat_w = _(u3a_is_north(u3R)) ? u3R->hat_w - u3R->rut_w
                                : u3R->rut_w - u3R->hat_w;

  for ( i_w = 0; i_w < u3_cc_fbox_no; i_w++ ) {
    u3a_fbox* fre_u = u3R->all.fre_u[i_w];

    while ( fre_u ) {
      fre_w += fre_u->box_u.siz_w;
      fre_u = fre_u->nex_u;
    }
  }
  u3l_log("dump: hat_w %x, fre_w %x, allocated %x",
          hat_w, fre_w, (hat_w - fre_w));

  if ( 0 != (hat_w - fre_w) ) {
    c3_w* box_w = _(u3a_is_north(u3R)) ? u3R->rut_w : u3R->hat_w;
    c3_w  mem_w = 0;

    while ( box_w < (_(u3a_is_north(u3R)) ? u3R->hat_w : u3R->rut_w) ) {
      u3a_box* box_u = (void *)box_w;

      if ( 0 != box_u->use_w ) {
#ifdef U3_MEMORY_DEBUG
        // u3l_log("live %d words, code %x", box_u->siz_w, box_u->cod_w);
#endif
        mem_w += box_u->siz_w;
      }
      box_w += box_u->siz_w;
    }

    u3l_log("second count: %x", mem_w);
  }
}
#endif

struct bt_cb_data {
  c3_y  count;
  c3_y  fail;
  c3_c* pn_c;
};

static void
err_cb(void* data, const char* msg, int errnum)
{
  struct bt_cb_data* bdata = (struct bt_cb_data *)data;
  bdata->count++;

  if ( bdata->count <= 1 ) {
    /* u3l_log("Backtrace error %d: %s", errnum, msg); */
    bdata->fail = 1;
  }
}

static int
bt_cb(void* data,
      uintptr_t pc,
      const char* filename,
      int lineno,
      const char* function)
{
  #ifndef U3_OS_windows
  struct bt_cb_data* bdata = (struct bt_cb_data *)data;
  bdata->count++;

  Dl_info info = {};
  c3_c*   fname_c = {0};

  if ( dladdr((void *)pc, &info) ) {
    for ( c3_w i_w = 0; info.dli_fname[i_w] != 0; i_w++ )
      if ( info.dli_fname[i_w] == '/' ) {
        fname_c = (c3_c*)&info.dli_fname[i_w + 1];
      }
  }

  if ( bdata->count <= 100 ) {
    c3_c* loc[128];
    if (filename != 0) {
      snprintf((c3_c*)loc, 128, "%s:%d", filename, lineno);
    }
    else {
      snprintf((c3_c*)loc, 128, "%s", fname_c != 0 ? fname_c : "-");
    }

    c3_c* fn_c;
    if (function != 0 || bdata->pn_c != 0) {
      fn_c = (c3_c*)(function != 0 ? function : bdata->pn_c);
    }
    else {
      fn_c = (c3_c*)(info.dli_sname != 0 ? info.dli_sname : "-");
    }

    fprintf(stderr, "%-3d %-35s %s\r\n", bdata->count - 1, fn_c, (c3_c *)loc);

    bdata->pn_c = 0;
    return 0;
  }
  else {
    bdata->pn_c = 0;
    return 1;
  }
  #endif
  return 0;
}

/* _self_path(): get binary self-path.
 */
static c3_y
_self_path(c3_c *pat_c)
{
  c3_i len_i = 0;
  c3_i pat_i;

  if ( 0 < (len_i = wai_getExecutablePath(NULL, 0, &pat_i)) ) {
    wai_getExecutablePath(pat_c, len_i, &pat_i);
    pat_c[len_i] = 0;
    return 0;
  }

  return 1;
}

void
u3m_stacktrace()
{
#ifndef U3_OS_windows
  void* bt_state;
  struct bt_cb_data data = { 0, 0, 0 };
  c3_c* self_path_c[4096] = {0};

#if defined(U3_OS_osx)
  fprintf(stderr, "Stacktrace:\r\n");

  if ( _self_path((c3_c*)self_path_c) == 0 ) {
    bt_state = backtrace_create_state((const c3_c*)self_path_c, 0, err_cb, 0);
    backtrace_full(bt_state, 0, bt_cb, err_cb, &data);
    if (data.fail == 0) {
      fprintf(stderr, "\r\n");
    }
  }
  else {
    data.fail = 1;
  }

  if ( data.fail == 1 ) {
    void*  array[100];
    c3_c** strings;
    size_t size = backtrace(array, 100);

    strings = backtrace_symbols(array, size);

    if ( strings[0] == NULL ) {
      fprintf(stderr, "Backtrace failed\r\n");
    }
    else {
      for ( c3_i i = 0; i < size; i++ ) {
        fprintf(stderr, "%s\r\n", strings[i]);
      }
      fprintf(stderr, "\r\n");
    }

    free(strings);
  }
#elif defined(U3_OS_linux)
  /* TODO: Fix unwind not getting past signal trampoline on linux aarch64
   */
  fprintf(stderr, "Stacktrace:\r\n");

  if ( _self_path((c3_c*)self_path_c) == 0 ) {
    bt_state = backtrace_create_state((const c3_c*)self_path_c, 0, err_cb, 0);

    unw_context_t context;
    unw_cursor_t cursor;
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    unw_word_t pc, sp;

    c3_c* pn_c[1024] = {0};
    c3_w  offp_w = 0;

    do {
      unw_get_reg(&cursor, UNW_REG_IP, &pc);
      unw_get_reg(&cursor, UNW_REG_SP, &sp);
      if ( 0 == unw_get_proc_name(&cursor, (c3_c*)pn_c, 1024, (unw_word_t *)&offp_w) )
        data.pn_c = (c3_c*)pn_c;
      backtrace_pcinfo(bt_state, pc - 1, bt_cb, err_cb, &data);
    } while (unw_step(&cursor) > 0);

    if ( (data.count > 0) ) {
      fprintf(stderr, "\r\n");
    }
  }
  else {
    data.fail = 1;
    fprintf(stderr, "Backtrace failed\r\n");
  }
#endif
#endif
}

/* u3m_bail(): bail out.  Does not return.
**
**  Bail motes:
**
**    %evil               ::  erroneous cryptography
**    %exit               ::  semantic failure
**    %oops               ::  assertion failure
**    %intr               ::  interrupt
**    %fail               ::  computability failure
**    %over               ::  stack overflow (a kind of %fail)
**    %meme               ::  out of memory
**
**  These are equivalents of the full exception noun, the error ball:
**
**    $%  [%0 success]
**        [%1 paths]
**        [%2 trace]
**        [%3 code trace]
**    ==
**
**  XX several of these abort() calls should be gated by -a
*/
c3_i
u3m_bail(u3_noun how)
{
  //  printf some metadata
  //
  switch ( how ) {
    case c3__evil:
    case c3__exit: break;

    default: {
      if ( _(u3ud(how)) ) {
        c3_c str_c[5];

        str_c[0] = ((how >>  0) & 0xff);
        str_c[1] = ((how >>  8) & 0xff);
        str_c[2] = ((how >> 16) & 0xff);
        str_c[3] = ((how >> 24) & 0xff);
        str_c[4] = 0;
        fprintf(stderr, "\r\nbail: %s\r\n", str_c);
      }
      else if ( 1 != u3h(how) ) {
        u3_assert(_(u3ud(u3h(how))));
        fprintf(stderr, "\r\nbail: %d\r\n", u3h(how));
      }
    }
  }

  if ( &(u3H->rod_u) == u3R ) {
    //  XX set exit code
    //
    fprintf(stderr, "home: bailing out\r\n\r\n");
    u3m_stacktrace();
    abort();
  }

  //  intercept fatal errors
  //
  switch ( how ) {
    case c3__foul:
    case c3__oops: {
      //  XX set exit code
      //
      fprintf(stderr, "bailing out\r\n\r\n");
      u3m_stacktrace();
      abort();
    }
  }

  if ( &(u3H->rod_u) == u3R ) {
    //  For top-level errors, which shouldn't happen often, we have no
    //  choice but to use the signal process; and we require the flat
    //  form of how.
    //
    //    XX JB: these seem unrecoverable, at least wrt memory management,
    //    so they've been disabled above for now
    //
    u3_assert(_(u3a_is_cat(how)));
    u3m_signal(how);
  }

  //  release the emergency buffer, ensuring space for cells
  //
  u3z(u3R->bug.mer);
  u3R->bug.mer = 0;

  /* Reconstruct a correct error ball.
  */
  if ( _(u3ud(how)) ) {
    switch ( how ) {
      case c3__exit: {
        how = u3nc(2, u3R->bug.tax);
      } break;

      default: {
        how = u3nt(3, how, u3R->bug.tax);
      } break;
    }
  }

  // Reset the spin stack pointer
  if ( NULL != stk_u ) {
    stk_u->off_w = u3R->off_w;
    stk_u->fow_w = u3R->fow_w;
  }

  _longjmp(u3R->esc.buf, how);
}

int c3_cooked(void) { return u3m_bail(c3__oops); }

/* u3m_error(): bail out with %exit, ct_pushing error.
*/
c3_i
u3m_error(c3_c* str_c)
{
  u3t_mean(u3i_string(str_c));
  return u3m_bail(c3__exit);
}

/* u3m_leap(): in u3R, create a new road within the existing one.
*/
void
u3m_leap(c3_w pad_w)
{
  u3_road* rod_u;

  _rod_vaal(u3R);

  //  push a new road struct onto the stack
  //
  {
    u3a_pile pil_u;
    c3_p     ptr_p;
    u3a_pile_prep(&pil_u, sizeof(u3a_road) + 15); // XX refactor to wiseof
    ptr_p = (c3_p)u3a_push(&pil_u);

    //  XX add push_once, push_once_aligned
    //
    if ( ptr_p & 15 ) {
      ptr_p &= ~15;
      if ( c3n == u3a_is_north(u3R) ) {
        ptr_p += 16;
      }
    }

    rod_u = (void*)ptr_p;
    memset(rod_u, 0, sizeof(u3a_road));
  }

  /* Allocate a region on the cap.
  */
  {
    u3p(c3_w) bot_p, top_p;            /* S: bot_p = new mat. N: bot_p = new rut  */

    if ( c3y == u3a_is_north(u3R) ) {
      //  pad and page-align the hat
      //
      bot_p  = u3R->hat_p + pad_w;
      bot_p +=   (1U << u3a_page) - 1;
      bot_p &= ~((1U << u3a_page) - 1);
      top_p  = u3R->cap_p;
      top_p &= ~((1U << u3a_page) - 1);

      if ( bot_p >= top_p ) {
        u3m_bail(c3__meme);
      }

      u3e_ward(bot_p - 1, top_p);
      rod_u->mat_p = rod_u->cap_p = bot_p;
      rod_u->rut_p = rod_u->hat_p = top_p;

      //  in a south road, the heap is high and the stack is low
      //
      //    the heap starts at the end of the memory segment;
      //    the stack starts at the base memory pointer [mem_w],
      //    and ends after the space for the road structure [siz_w]
      //
      //    00~~~|M|+++|C|######|H|---|R|~~~FFF
      //         ^---u3R which _pave_road returns
      //
      //    XX obsolete?

      _rod_vaal(rod_u);
#if 0
      fprintf(stderr, "NPAR.hat_p: 0x%x %p, SKID.hat_p: 0x%x %p\r\n",
              u3R->hat_p, u3a_into(u3R->hat_p),
              rod_u->hat_p, u3a_into(rod_u->hat_p));
#endif
    }
    else {
      bot_p  = u3R->cap_p;
      bot_p +=   (1U << u3a_page) - 1;
      bot_p &= ~((1U << u3a_page) - 1);
      top_p  = u3R->hat_p - pad_w;
      top_p &= ~((1U << u3a_page) - 1);

      //  XX moar
      if ( (u3R->hat_p < pad_w) || (bot_p >= top_p) ) {
        u3m_bail(c3__meme);
      }

      u3e_ward(bot_p - 1, top_p);
      rod_u->rut_p = rod_u->hat_p = bot_p;
      rod_u->mat_p = rod_u->cap_p = top_p;

      //  in a north road, the heap is low and the stack is high
      //
      //    the heap starts at the base memory pointer [mem_w];
      //    the stack starts at the end of the memory segment,
      //    minus space for the road structure [siz_w]
      //
      //    00~~~|R|---|H|######|C|+++|M|~~~FF
      //                                ^--u3R which _pave_road returns (u3H for home road)
      //
      //    XX obsolete?

      _rod_vaal(rod_u);

#if 0
      fprintf(stderr, "SPAR.hat_p: 0x%x %p, NKID.hat_p: 0x%x %p\r\n",
              u3R->hat_p, u3a_into(u3R->hat_p),
              rod_u->hat_p, u3a_into(rod_u->hat_p));

#endif
    }
  }

  /* Attach the new road to its parents.
  */
  {
    u3_assert(0 == u3R->kid_p);
    rod_u->par_p = u3of(u3_road, u3R);
    u3R->kid_p = u3of(u3_road, rod_u);
  }

  // Add slow stack pointer to rod_u
  if ( NULL != stk_u ) {
    rod_u->off_w = stk_u->off_w;
    rod_u->fow_w = stk_u->fow_w;
  } 

  /* Set up the new road.
  */
  {
    u3R = rod_u;
    _pave_parts();
  }
#ifdef U3_MEMORY_DEBUG
  rod_u->all.fre_w = 0;
#endif

  _rod_vaal(u3R);
}

void
_print_diff(c3_c* cap_c, c3_w a, c3_w b)
{
  c3_w diff = a<b ? b-a : a-b;
  u3a_print_memory(stderr, cap_c, diff);
}

/* u3m_fall(): in u3R, return an inner road to its parent.
*/
void
u3m_fall(void)
{
  u3_assert(0 != u3R->par_p);

#if 0
  /*  If you're printing a lot of these you need to change
   *  u3a_print_memory from fprintf to u3l_log
  */
  fprintf(stderr, "fall: from %s %p, to %s %p (cap 0x%x, was 0x%x)\r\n",
          _(u3a_is_north(u3R)) ? "north" : "south",
          (void*)u3R,
          _(u3a_is_north(u3to(u3_road, u3R->par_p))) ? "north" : "south",
          u3to(void, u3R->par_p),
          u3R->hat_p,
          u3R->rut_p);
  _print_diff("unused free", u3R->hat_p, u3R->cap_p);
  _print_diff("freeing", u3R->rut_p, u3R->hat_p);
  _print_diff("stack", u3R->cap_p, u3R->mat_p);
  static c3_w wat_w = 500000000;
  if (u3to(u3_road, u3R->par_p) == &u3H->rod_u) {
    wat_w = 500000000;
  }
  else {
    wat_w = c3_min(wat_w,
                   u3R->hat_p < u3R->cap_p ?
                     u3R->cap_p - u3R->hat_p :
                     u3R->hat_p - u3R->cap_p);
  }
  u3a_print_memory(stderr, "low water mark", wat_w);

#endif

  u3to(u3_road, u3R->par_p)->pro.nox_d += u3R->pro.nox_d;
  u3to(u3_road, u3R->par_p)->pro.cel_d += u3R->pro.cel_d;

  /* The new cap is the old hat - it's as simple as that.
  */
  u3to(u3_road, u3R->par_p)->cap_p = u3R->hat_p;

  /* And, we're back home.
  */
  u3R = u3to(u3_road, u3R->par_p);
  u3R->kid_p = 0;
}

/* u3m_hate(): new, integrated leap mechanism (enter).
*/
void
u3m_hate(c3_w pad_w)
{
  u3_assert(0 == u3R->ear_p);

  u3R->ear_p = u3R->cap_p;
  u3m_leap(pad_w);

  u3R->bug.mer = u3i_string(
    "emergency buffer with sufficient space to cons the trace and bail"
  );
}

//  RETAINS `now`.
//
static void
_m_renew_timer(u3_atom now)
{
  u3_atom min = u3_nul;
  u3a_road* rod_u = u3R;
  c3_t no_timers_t = true;
  while ( 1 ) {
    for (u3_noun l = rod_u->tim; l; l = u3t(l)) {
      no_timers_t = false;
      u3_atom fut = u3h(l);
      if ( _(u3qa_gth(fut, now)) ) {
        min = ( u3_nul == min ) ? u3k(fut) : u3ka_min(min, u3k(fut));
      }
      else {
        //  we are waiting for the signal to come, do nothing
        //
        u3z(min);
        return;
      }
    }
    if ( !rod_u->par_p ) break;
    rod_u = u3to(u3_road, rod_u->par_p);
  }

  if ( no_timers_t ) {
    //  no timers: `min` is still u3_nul.
    //  disarm the timer
    //
    struct itimerval itm_u;
    timerclear(&itm_u.it_interval);
    timerclear(&itm_u.it_value);
    if ( rsignal_setitimer(ITIMER_VIRTUAL, &itm_u, 0) ) {
      u3l_log("loom: clear timer failed %s", strerror(errno));
    }
    return;
  }

  if ( u3_nul == min ) {
    //  strange case: `now` is later or equal to all our deadlines. do nothing
    //
    return;
  }

  u3_atom gap = u3ka_sub(min, u3k(now));
  
  struct itimerval itm_u;
  timerclear(&itm_u.it_interval);
  c3_t is_set_t = u3m_time_out_it(&itm_u, gap);
  if ( !is_set_t ) {
    //  the gap is too small to resolve in itimerval, emulate firing SIGALRM
    // immediately
    //
    u3m_signal(c3__alrm);
  }
  if ( rsignal_setitimer(ITIMER_VIRTUAL, &itm_u, 0) ) {
    u3l_log("loom: set timer failed %s", strerror(errno));
  }
}

static void
_m_renew_now(void)
{
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  u3_atom now = u3m_time_in_tv(&tim_u);
  _m_renew_timer(now);
  u3z(now);
}

/* u3m_timer_set(): push a new timer to the timer stack.
** gap is @dr, gap != 0
*/
void
u3m_timer_set(u3_atom gap)
{
  if ( !u3R->par_p ) {
    //  noop on the home road since we have no jump buffer
    //
    u3z(gap);
    return;
  }
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  u3_atom now = u3m_time_in_tv(&tim_u);
  u3_atom fut = u3ka_add(u3k(now), gap);
  u3R->tim = u3nc(fut, u3R->tim);
  _m_renew_timer(now);
  u3z(now);
}

/* u3m_timer_pop(): pop a timer off the timer stack.
** timer stack must be non-empty
*/
void
u3m_timer_pop(void)
{
  if ( !u3R->par_p ) {
    //  noop on the home road since we have no jump buffer
    //
    return;
  }
  c3_dessert( c3y == u3du(u3R->tim) );
  u3_noun t = u3k(u3t(u3R->tim));
  u3z(u3R->tim), u3R->tim = t;
  _m_renew_now();
}

/* u3m_love(): return product from leap.
*/
u3_noun
u3m_love(u3_noun pro)
{
  //  save cache pointers from current road
  //
  u3p(u3h_root) byc_p = u3R->byc.har_p;
  u3a_jets      jed_u = u3R->jed;
  u3p(u3h_root) per_p = u3R->cax.per_p;

  //  are there any timers on the road?
  //
  c3_o tim_o = u3du(u3R->tim);

  //  fallback to parent road (child heap on parent's stack)
  //
  u3m_fall();

  if ( _(tim_o) ) _m_renew_now();

  //  copy product and caches off our stack
  //
  pro   = u3a_take(pro);
  jed_u = u3j_take(jed_u);
  byc_p = u3n_take(byc_p);
  per_p = u3h_take(per_p);

  //  pop the stack
  //
  u3a_drop_heap(u3R->cap_p, u3R->ear_p);
  u3R->cap_p = u3R->ear_p;
  u3R->ear_p = 0;

  //  integrate junior caches
  //
  u3j_reap(jed_u);
  u3n_reap(byc_p);
  u3z_reap(u3z_memo_keep, per_p);

  return pro;
}

/* u3m_golf(): record cap_p length for u3m_flog().
*/
c3_w
u3m_golf(void)
{
  if ( c3y == u3a_is_north(u3R) ) {
    return u3R->mat_p - u3R->cap_p;
  }
  else {
    return u3R->cap_p - u3R->mat_p;
  }
}

/* u3m_flog(): reset cap_p.
*/
void
u3m_flog(c3_w gof_w)
{
  //  Enable memsets in case of memory corruption.
  //
  if ( c3y == u3a_is_north(u3R) ) {
    u3_post bot_p = (u3R->mat_p - gof_w);
    // c3_w  len_w = (bot_w - u3R->cap_w);

    // memset(u3R->cap_w, 0, 4 * len_w);
    u3R->cap_p = bot_p;
  }
  else {
    u3_post bot_p = u3R->mat_p + gof_w;
    // c3_w  len_w = (u3R->cap_w - bot_w);

    // memset(bot_w, 0, 4 * len_w);   //
    u3R->cap_p = bot_p;
  }
}

/* u3m_water(): produce watermarks.
*/
void
u3m_water(u3_post* low_p, u3_post* hig_p)
{
  //  allow the segfault handler to fire before the road is set
  //
  //    while not explicitly possible in the codebase,
  //    compiler optimizations can reorder stores
  //
  if ( !u3R ) {
    *low_p = 0;
    *hig_p = u3C.wor_i - 1;
  }
  //  in a north road, hat points to the end of the heap + 1 word,
  //  while cap points to the top of the stack
  //
  else if ( c3y == u3a_is_north(u3R) ) {
    *low_p = u3R->hat_p - 1;
    *hig_p = u3R->cap_p;
  }
  //  in a south road, hat points to the end of the heap,
  //  while cap points to the top of the stack + 1 word
  //
  else {
    *low_p = u3R->cap_p - 1;
    *hig_p = u3R->hat_p;
  }
}

/* u3m_soft_top(): top-level safety wrapper.
*/
u3_noun
u3m_soft_top(c3_w    mil_w,                     //  timer ms
             c3_w    pad_w,                     //  base memory pad
             u3_funk fun_f,
             u3_noun   arg)
{
  u3_noun why, pro;
  volatile c3_l sig_l = 0;

  /* Enter internal signal regime.
   */
  _cm_signal_deep();

  if ( 0 != (sig_l = rsignal_setjmp(u3_Signal)) ) {
    //  reinitialize trace state
    //
    u3t_init();

    //  return to blank state
    //
    _cm_signal_done();

    //  recover memory state from the top down
    //
    return _cm_signal_recover(sig_l, arg);
  }

  /* Record the cap, and leap.
  */
  u3m_hate(pad_w);

  if ( mil_w ) {
    u3m_timer_set(u3m_time_gap_in_mil(mil_w));
  }

  /* Trap for ordinary nock exceptions.
  */
  if ( 0 == (why = (u3_noun)_setjmp(u3R->esc.buf)) ) {
    pro = fun_f(arg);

    /* Make sure the inner routine did not create garbage.
    */
    if ( u3C.wag_w & u3o_debug_ram ) {
#ifdef U3_CPU_DEBUG
      if ( u3R->all.max_w > 1000000 ) {
        u3a_print_memory(stderr, "execute: top", u3R->all.max_w);
      }
#endif
      u3m_grab(pro, u3_none);
    }

    /* Revert to external signal regime.
    */
    _cm_signal_done();

    /* Produce success, on the old road.
    */
    pro = u3nc(0, u3m_love(pro));
  }
  else {
    /* Overload the error result.
    */
    pro = u3m_love(why);
  }

  /* Revert to external signal regime.
  */
  _cm_signal_done();

  /* Free the argument.
  */
  u3z(arg);

  /* Return the product.
  */
  return pro;
}

/* u3m_soft_sure(): top-level call assumed correct.
*/
u3_noun
u3m_soft_sure(u3_funk fun_f, u3_noun arg)
{
  u3_noun pro, pru = u3m_soft_top(0, (1 << 18), fun_f, arg);

  u3_assert(_(u3du(pru)));
  pro = u3k(u3t(pru));
  u3z(pru);

  return pro;
}

/* u3m_soft_slam: top-level call.
*/
u3_noun _cm_slam(u3_noun arg) { return u3n_slam_on(u3h(arg), u3t(arg)); }
u3_noun
u3m_soft_slam(u3_noun gat, u3_noun sam)
{
  return u3m_soft_sure(_cm_slam, u3nc(gat, sam));
}

/* u3m_soft_nock: top-level nock.
*/
u3_noun _cm_nock(u3_noun arg) { return u3n_nock_on(u3h(arg), u3t(arg)); }
u3_noun
u3m_soft_nock(u3_noun bus, u3_noun fol)
{
  return u3m_soft_sure(_cm_nock, u3nc(bus, fol));
}

static void
_hamt_map(u3_noun kev, void* cax_p)
{
  u3_noun* old = cax_p;
  u3_noun key, val, new;
  u3x_cell(kev, &key, &val);
  new = u3qdb_put(*old, u3t(key), val);
  u3z(*old);
  *old = new;
}

/* u3m_soft_cax(): descend into virtualization context, with cache.
*/
u3_noun
u3m_soft_cax(u3_funq fun_f,
             u3_noun aga,
             u3_noun agb)
{
  u3_noun why = 0, pro;
  u3_noun cax = u3_nul;

  /* Record the cap, and leap.
  */
  u3m_hate(1 << 18);

  u3R->how.fag_w |= u3a_flag_cash;

  /* Configure the new road.
  */
  {
    u3R->ski.gul = u3_nul;
    u3R->pro.don = u3to(u3_road, u3R->par_p)->pro.don;
    u3R->pro.trace = u3to(u3_road, u3R->par_p)->pro.trace;
    u3R->bug.tax = 0;
  }
  u3t_on(coy_o);

  /* Trap for exceptions.
  */
  if ( 0 == (why = (u3_noun)_setjmp(u3R->esc.buf)) ) {
    u3t_off(coy_o);
    pro = fun_f(aga, agb);

#ifdef U3_CPU_DEBUG
    if ( u3R->all.max_w > 1000000 ) {
      u3a_print_memory(stderr, "execute: run", u3R->all.max_w);
    }
#endif

    /* Today you can't run -g without memory debug, but you should be
     * able to.
    */
#ifdef U3_MEMORY_DEBUG
    if ( u3C.wag_w & u3o_debug_ram ) {
      u3m_grab(pro, u3_none);
    }
#endif

    /* Produce success, on the old road.
    */
    u3h_walk_with(u3R->cax.per_p, _hamt_map, &cax);
    pro = u3nc(u3nc(0, pro), cax);
    pro = u3m_love(pro);
  }
  else {
    u3t_init();

    /* Produce - or fall again.
    */
    {
      u3_assert(_(u3du(why)));
      switch ( u3h(why) ) {
        default: u3_assert(0); return 0;

        case 1: {                             //  blocking request
          pro = u3nc(u3nc(2, u3m_love(u3R->bug.tax)), u3_nul);
        } break;

        case 2: {                             //  true exit
          pro = u3nc(u3m_love(why), u3_nul);
        } break;

        case 3: {                             //  failure; rebail w/trace
          u3_noun yod = u3m_love(u3t(why));

          u3m_bail
            (u3nt(3,
                  u3a_take(u3h(yod)),
                  u3kb_weld(u3t(yod), u3k(u3R->bug.tax))));
        } break;
      }
    }
  }
  /* Release the arguments.
  */
  {
    u3z(aga);
    u3z(agb);
  }

  /* Return the product.
  */
  return pro;
}

/* u3m_soft_run(): descend into virtualization context.
*/
u3_noun
u3m_soft_run(u3_noun gul,
             u3_funq fun_f,
             u3_noun aga,
             u3_noun agb)
{
  u3_noun why = 0, pro;

  c3_t cash_t = !!(u3R->how.fag_w & u3a_flag_cash);

  /* Record the cap, and leap.
  */
  u3m_hate(1 << 18);

  /* Configure the new road.
  */

  {
    if ( (u3_nul == gul) || cash_t ) {
      u3R->ski.gul = u3_nul;
    }
    else {
      u3R->ski.gul = u3nc(gul, u3to(u3_road, u3R->par_p)->ski.gul);
    }
    u3R->pro.don = u3to(u3_road, u3R->par_p)->pro.don;
    u3R->pro.trace = u3to(u3_road, u3R->par_p)->pro.trace;
    u3R->bug.tax = 0;
    u3R->how.fag_w |= ( cash_t ) ? u3a_flag_cash : 0;
  }
  u3t_on(coy_o);

  /* Trap for exceptions.
  */
  if ( 0 == (why = (u3_noun)_setjmp(u3R->esc.buf)) ) {
    u3t_off(coy_o);
    pro = fun_f(aga, agb);

#ifdef U3_CPU_DEBUG
    if ( u3R->all.max_w > 1000000 ) {
      u3a_print_memory(stderr, "execute: run", u3R->all.max_w);
    }
#endif

    /* Today you can't run -g without memory debug, but you should be
     * able to.
    */
#ifdef U3_MEMORY_DEBUG
    if ( u3C.wag_w & u3o_debug_ram ) {
      u3m_grab(pro, u3_none);
    }
#endif

    /* Produce success, on the old road.
    */
    pro = u3nc(0, u3m_love(pro));
  }
  else {
    u3t_init();

    /* Produce - or fall again.
    */
    {
      u3_assert(_(u3du(why)));
      switch ( u3h(why) ) {
        default: u3_assert(0); return 0;

        case 0: {                             //  unusual: bail with success.
          pro = u3m_love(why);
        } break;

        case 1: {                             //  blocking request
          pro = u3m_love(why);
        } break;

        case 2: {                             //  true exit
          pro = u3m_love(why);
        } break;

        case 3: {                             //  failure; rebail w/trace
          u3_noun yod = u3m_love(u3t(why));

          u3m_bail
            (u3nt(3,
                  u3a_take(u3h(yod)),
                  u3kb_weld(u3t(yod), u3k(u3R->bug.tax))));
        } break;

        case 4: {                             //  meta-bail
          u3m_bail(u3m_love(u3t(why)));
        } break;
      }
    }
  }

  /* Release the arguments.
  */
  {
    u3z(gul);
    u3z(aga);
    u3z(agb);
  }

  /* Return the product.
  */
  return pro;
}

/* u3m_soft_esc(): namespace lookup.  Produces direct result.
*/
u3_noun
u3m_soft_esc(u3_noun ref, u3_noun sam)
{
  u3_noun why, gul, pro;

  /* Assert preconditions.
  */
  {
    gul = u3h(u3R->ski.gul);
  }

  /* Record the cap, and leap.
  */
  u3m_hate(1 << 18);

  /* Configure the new road.
  */
  {
    u3R->ski.gul = u3t(u3to(u3_road, u3R->par_p)->ski.gul);
    u3R->pro.don = u3to(u3_road, u3R->par_p)->pro.don;
    u3R->pro.trace = u3to(u3_road, u3R->par_p)->pro.trace;
    u3R->bug.tax = 0;
  }

  /* Trap for exceptions.
  */
  if ( 0 == (why = (u3_noun)_setjmp(u3R->esc.buf)) ) {
    pro = u3n_slam_on(gul, u3nc(ref, sam));

    /* Fall back to the old road, leaving temporary memory intact.
    */
    pro = u3m_love(pro);
  }
  else {
    u3t_init();

    /* Push the error back up to the calling context - not the run we
    ** are in, but the caller of the run, matching pure nock semantics.
    */
    u3m_bail(u3nc(4, u3m_love(why)));
  }

  /* Release the sample.  Note that we used it above, but in a junior
  ** road, so its refcount is intact.
  */
  u3z(ref);
  u3z(sam);

  /* Return the product.
  */
  return pro;
}

/* u3m_grab(): garbage-collect the world, plus extra roots.
*/
void
u3m_grab(u3_noun som, ...)   // terminate with u3_none
{
  // u3h_free(u3R->cax.har_p);
  // u3R->cax.har_p = u3h_new();

  u3a_mark_init();
  {
    va_list vap;
    u3_noun tur;

    va_start(vap, som);

    if ( som != u3_none ) {
      u3a_mark_noun(som);

      while ( u3_none != (tur = va_arg(vap, u3_noun)) ) {
        u3a_mark_noun(tur);
      }
    }
    va_end(vap);
  }
  u3m_mark(); // XX leaks
  u3a_sweep();
}

/* u3m_soft(): top-level wrapper.
**
** Produces [0 product] or [%error (list tank)], top last.
*/
u3_noun
u3m_soft(c3_w    mil_w,
         u3_funk fun_f,
         u3_noun   arg)
{
  u3_noun why;

  why = u3m_soft_top(mil_w, (1 << 20), fun_f, arg);   // 4M pad

  if ( 0 == u3h(why) ) {
    return why;
  }
  else {
    u3_noun tax, cod, pro;

    switch ( u3h(why) ) {
      case 2: {
        cod = c3__exit;
        tax = u3t(why);
      } break;

      case 3: {
        cod = u3h(u3t(why));
        tax = u3t(u3t(why));
      } break;

      //  don't use .^ at the top level!
      //
      default: {
        u3m_p("invalid mot", u3h(why));
        u3_assert(0);
      }
    }

    //  don't call +mook if we have no kernel
    //
    //    This is required to soft the boot sequence.
    //
    if ( 0 == u3A->roc ) {
      while ( u3_nul != tax ) {
        u3_noun dat, mot, val;
        u3x_cell(tax, &dat, &tax);

        if ( c3y == u3r_cell(dat, &mot, &val) ) {
          if ( c3__spot == mot ) {
            u3m_p("tax", val);
          }
          else if (  (c3__mean == mot)
                  && (c3y == u3a_is_atom(val)) )
          {
            u3m_p("men", val);
          }
          else {
            u3m_p("mot", mot);
          }
        }
      }

      pro = u3nc(u3k(cod), u3_nul);
    }
    //  %evil leaves no trace
    //
    else if ( c3__evil == cod ) {
      pro = u3nc(u3k(cod), u3_nul);
    }
    else {
      u3_noun mok = u3dc("mook", 2, u3k(tax));
      pro = u3nc(u3k(cod), u3k(u3t(mok)));
      u3z(mok);
    }

    u3z(why);
    return pro;
  }
}

/* _cm_is_tas(): yes iff som (RETAIN) is @tas.
*/
static c3_o
_cm_is_tas(u3_atom som, c3_w len_w)
{
  c3_w i_w;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_c c_c = u3r_byte(i_w, som);

    if ( islower(c_c) ||
        (isdigit(c_c) && (0 != i_w) && ((len_w - 1) != i_w))
        || '-' == c_c )
    {
      continue;
    }
    return c3n;
  }
  return c3y;
}

/* _cm_is_ta(): yes iff som (RETAIN) is @ta.
*/
static c3_o
_cm_is_ta(u3_noun som, c3_w len_w)
{
  c3_w i_w;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_c c_c = u3r_byte(i_w, som);

    if ( (c_c < 32) || (c_c > 127) ) {
      return c3n;
    }
  }
  return c3y;
}

/* _cm_hex(): hex byte.
*/
c3_y _cm_hex(c3_y c_y)
{
  if ( c_y < 10 )
    return '0' + c_y;
  else return 'a' + (c_y - 10);
}

/* _cm_in_pretty: measure/cut prettyprint.
*/
static c3_w
_cm_in_pretty(u3_noun som, c3_o sel_o, c3_c* str_c)
{
  if ( _(u3du(som)) ) {
    c3_w sel_w, one_w, two_w;

    sel_w = 0;
    if ( _(sel_o) ) {
      if ( str_c ) { *(str_c++) = '['; }
      sel_w += 1;
    }

    one_w = _cm_in_pretty(u3h(som), c3y, str_c);
    if ( str_c ) {
      str_c += one_w;
      *(str_c++) = ' ';
    }
    two_w = _cm_in_pretty(u3t(som), c3n, str_c);
    if ( str_c ) { str_c += two_w; }

    if ( _(sel_o) ) {
      if ( str_c ) { *(str_c++) = ']'; }
      sel_w += 1;
    }
    return one_w + two_w + 1 + sel_w;
  }
  else {
    if ( som < 65536 ) {
      c3_c buf_c[6];
      c3_w len_w;

      snprintf(buf_c, 6, "%d", som);
      len_w = strlen(buf_c);

      if ( str_c ) { strcpy(str_c, buf_c); str_c += len_w; }
      return len_w;
    }
    else {
      c3_w len_w = u3r_met(3, som);

      if ( _(_cm_is_tas(som, len_w)) ) {
        if ( str_c ) {
          *(str_c++) = '%';
          u3r_bytes(0, len_w, (c3_y *)str_c, som);
          str_c += len_w;
        }
        return len_w + 1;
      }
      else if ( _(_cm_is_ta(som, len_w)) ) {
        if ( str_c ) {
          *(str_c++) = '\'';
          u3r_bytes(0, len_w, (c3_y *)str_c, som);
          str_c += len_w;
          *(str_c++) = '\'';
        }
        return len_w + 2;
      }
      else {
        c3_c *buf_c = c3_malloc(2 + (2 * len_w) + 1);
        c3_w i_w = 0;
        c3_w a_w = 0;

        buf_c[a_w++] = '0';
        buf_c[a_w++] = 'x';

        for ( i_w = 0; i_w < len_w; i_w++ ) {
          c3_y c_y = u3r_byte(len_w - (i_w + 1), som);

          if ( (i_w == 0) && (c_y <= 0xf) ) {
            buf_c[a_w++] = _cm_hex(c_y);
          } else {
            buf_c[a_w++] = _cm_hex(c_y >> 4);
            buf_c[a_w++] = _cm_hex(c_y & 0xf);
          }
        }
        buf_c[a_w] = 0;
        len_w = a_w;

        if ( str_c ) { strcpy(str_c, buf_c); str_c += len_w; }

        c3_free(buf_c);
        return len_w;
      }
    }
  }
}

/* u3m_pretty(): dumb prettyprint to string.
*/
c3_c*
u3m_pretty(u3_noun som)
{
  c3_w len_w = _cm_in_pretty(som, c3y, 0);
  c3_c* pre_c = c3_malloc(len_w + 1);

  _cm_in_pretty(som, c3y, pre_c);
  pre_c[len_w] = 0;
  return pre_c;
}

/* u3m_pretty_road(): dumb prettyprint to string. Road allocation
*/
c3_c*
u3m_pretty_road(u3_noun som)
{
  c3_w len_w = _cm_in_pretty(som, c3y, 0);
  c3_c* pre_c = u3a_malloc(len_w + 1);

  _cm_in_pretty(som, c3y, pre_c);
  pre_c[len_w] = 0;
  return pre_c;
}

/*  _cm_in_pretty_path: measure/cut prettyprint.
 *
 *  Modeled after _cm_in_pretty(), the backend to u3m_p(), but with the
 *  assumption that we're always displaying a path.
 */
static c3_w
_cm_in_pretty_path(u3_noun som, c3_c* str_c)
{
  if ( _(u3du(som)) ) {
    c3_w sel_w, one_w, two_w;
    if ( str_c ) {
      *(str_c++) = '/';
    }
    sel_w = 1;

    one_w = _cm_in_pretty_path(u3h(som), str_c);
    if ( str_c ) {
      str_c += one_w;
    }

    two_w = _cm_in_pretty_path(u3t(som), str_c);
    if ( str_c ) {
      str_c += two_w;
    }

    return sel_w + one_w + two_w;
  }
  else {
    c3_w len_w = u3r_met(3, som);
    if ( str_c && len_w ) {
      u3r_bytes(0, len_w, (c3_y *)str_c, som);
      str_c += len_w;
    }
    return len_w;
  }
}

/* u3m_pretty_path(): prettyprint a path to string.
*/
c3_c*
u3m_pretty_path(u3_noun som)
{
  c3_w len_w = _cm_in_pretty_path(som, NULL);
  c3_c* pre_c = c3_malloc(len_w + 1);

  _cm_in_pretty_path(som, pre_c);
  pre_c[len_w] = 0;
  return pre_c;
}

/* u3m_p(): dumb print with caption.
*/
void
u3m_p(const c3_c* cap_c, u3_noun som)
{
  c3_c* pre_c = u3m_pretty(som);

  u3l_log("%s: %s", cap_c, pre_c);
  c3_free(pre_c);
}

/* u3m_tape(): dump a tape to stdout.
*/
void
u3m_tape(u3_noun tep)
{
  u3_noun tap = tep;

  while ( u3_nul != tap ) {
    c3_c car_c;

    if ( u3h(tap) >= 127 ) {
      car_c = '?';
    } else car_c = u3h(tap);

    putc(car_c, stdout);
    tap = u3t(tap);
  }
  u3z(tep);
}

/* u3m_wall(): dump a wall to stdout.
*/
void
u3m_wall(u3_noun wol)
{
  u3_noun wal = wol;

  while ( u3_nul != wal ) {
    u3m_tape(u3k(u3h(wal)));

    putc(13, stdout);
    putc(10, stdout);

    wal = u3t(wal);
  }
  u3z(wol);
}

/* _cm_limits(): set up global modes and limits.
*/
static void
_cm_limits(void)
{
#ifndef U3_OS_windows
  struct rlimit rlm;

  //  Moar stack.
  //
  {
    u3_assert( 0 == getrlimit(RLIMIT_STACK, &rlm) );

    rlm.rlim_cur = c3_min(rlm.rlim_max, (65536 << 10));

    if ( 0 != setrlimit(RLIMIT_STACK, &rlm) ) {
      u3l_log("boot: stack size: %s", strerror(errno));
      exit(1);
    }
  }

  //  Moar filez.
  //
  {
    getrlimit(RLIMIT_NOFILE, &rlm);

  #ifdef U3_OS_osx
    rlm.rlim_cur = c3_min(OPEN_MAX, rlm.rlim_max);
  #else
    rlm.rlim_cur = rlm.rlim_max;
  #endif

    //  no exit, not a critical limit
    //
    if ( 0 != setrlimit(RLIMIT_NOFILE, &rlm) ) {
      u3l_log("boot: open file limit: %s", strerror(errno));
    }
  }

  // Moar core.
  //
# ifndef ASAN_ENABLED
  {
    getrlimit(RLIMIT_CORE, &rlm);
    rlm.rlim_cur = RLIM_INFINITY;

    //  no exit, not a critical limit
    //
    if ( 0 != setrlimit(RLIMIT_CORE, &rlm) ) {
      u3l_log("boot: core limit: %s", strerror(errno));
    }
  }
# endif
#endif
}

/* u3m_fault(): handle a memory event with libsigsegv protocol.
 */
c3_i
u3m_fault(void* adr_v, c3_i ser_i)
{
  c3_w*   adr_w = (c3_w*)adr_v;
  u3_post low_p, hig_p;

  //  let the stack overflow handler run.
  //
  if ( 0 == ser_i ) {
    return 0;
  }
  //  this could be avoided by registering the loom bounds in libsigsegv
  //
  else if ( (adr_w < u3_Loom) || (adr_w >= (u3_Loom + u3C.wor_i)) ) {
    fprintf(stderr, "loom: external fault: %p (%p : %p)\r\n\r\n",
            (void *)adr_w, (void *)u3_Loom, (void *)(u3_Loom + u3C.wor_i));
    u3m_stacktrace();
    u3_assert(0);
    return 0;
  }

  u3m_water(&low_p, &hig_p);

  switch ( u3e_fault(low_p, hig_p, u3a_outa(adr_w)) ) {
    //  page tracking invariants violated, fatal
    //
    case u3e_flaw_sham: {
      u3_assert(0);
      return 0;
    }

    //  virtual memory failure (protections)
    //
    //    XX s/b recoverable, need to u3m_signal() a new mote
    //
    case u3e_flaw_base: {
      u3_assert(0);
      return 0;
    }

    //  loom limits exceeded, recoverable
    //
    case u3e_flaw_meme: {
      u3m_signal(c3__meme); // doesn't return
      return 1;
    }

    case u3e_flaw_good: return 1;
  }

  u3_assert(!"unpossible");
}

/* u3m_foul(): dirty all pages and disable tracking.
*/
void
u3m_foul(void)
{
  if ( c3n == u3e_yolo() ) {
    return;
  }

  u3e_foul();
}

/* u3m_save(): update the checkpoint.
*/
void
u3m_save(void)
{
  u3_post low_p, hig_p;
  u3m_water(&low_p, &hig_p);

  u3a_wait();

  u3_assert(u3R == &u3H->rod_u);

  u3e_save(low_p, hig_p);

  u3a_dash();
}

/* u3m_toss(): discard ephemeral memory.
*/
void
u3m_toss(void)
{
  u3_post low_p, hig_p;
  u3m_water(&low_p, &hig_p);

  if (  ((low_p + u3C.tos_w) < u3C.wor_i)
     && (hig_p > u3C.tos_w) )
  {
    low_p += u3C.tos_w;
    hig_p -= u3C.tos_w;

    if ( low_p < hig_p ) {
      u3e_toss(low_p, hig_p);
    }
  }
}

/* u3m_ward(): tend the guardpage.
*/
void
u3m_ward(void)
{
  u3_post low_p, hig_p;
  u3m_water(&low_p, &hig_p);

#if 1  // XX redundant
  {
    c3_w low_w, hig_w;

    if ( c3y == u3a_is_north(u3R) ) {
      low_w = u3R->hat_p;
      hig_w = u3R->cap_p;
    }
    else {
      low_w = u3R->cap_p;
      hig_w = u3R->hat_p;
    }

    if (  (low_w > (u3P.gar_w << u3a_page))
       || (hig_w < (u3P.gar_w << u3a_page)) )
    {
      u3_assert(  ((low_p >> u3a_page) >= u3P.gar_w)
               || ((hig_p >> u3a_page) <= u3P.gar_w) );
    }
  }
#endif

  u3e_ward(low_p, hig_p);
}

/* _cm_signals(): set up interrupts, etc.
*/
static void
_cm_signals(void)
{
#ifndef U3_OS_windows
  if ( 0 != sigsegv_install_handler(u3m_fault) ) {
    u3l_log("boot: sigsegv install failed");
    exit(1);
  }

# if defined(U3_OS_PROF)
  //  Block SIGPROF, so that if/when we reactivate it on the
  //  main thread for profiling, we won't get hits in parallel
  //  on other threads.
  if ( u3C.wag_w & u3o_debug_cpu ) {
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGPROF);

    if ( 0 != pthread_sigmask(SIG_BLOCK, &set, NULL) ) {
      u3l_log("boot: thread mask SIGPROF: %s", strerror(errno));
      exit(1);
    }
  }
#endif
#else
  if (0 == AddVectoredExceptionHandler(1, _windows_exception_filter)) {
    u3l_log("boot: vectored exception handler install failed");
    exit(1);
  }
#endif
}

/* _cm_malloc_ssl(): openssl-shaped malloc
*/
static void*
_cm_malloc_ssl(size_t len_i
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
               , const char* file, int line
#endif
               )
{
  return u3a_malloc(len_i);
}

/* _cm_realloc_ssl(): openssl-shaped realloc.
*/
static void*
_cm_realloc_ssl(void* lag_v, size_t len_i
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
                , const char* file, int line
#endif
                )
{
  return u3a_realloc(lag_v, len_i);
}

/* _cm_free_ssl(): openssl-shaped free.
*/
static void
_cm_free_ssl(void* tox_v
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
             , const char* file, int line
#endif
             )
{
  u3a_free(tox_v);
}

extern void u3je_secp_init(void);

/* _cm_crypto(): initialize openssl and crypto jets.
*/
static void
_cm_crypto(void)
{
  /* Initialize OpenSSL with loom allocation functions. */
#ifndef U3_URTH_MASS
  if ( 0 == CRYPTO_set_mem_functions(&_cm_malloc_ssl,
                                     &_cm_realloc_ssl,
                                     &_cm_free_ssl) ) {
    u3l_log("%s", "openssl initialization failed");
    abort();
  }
#endif

  u3je_secp_init();
}

/* _cm_realloc2(): gmp-shaped realloc.
*/
static void*
_cm_realloc2(void* lag_v, size_t old_i, size_t new_i)
{
  return u3a_realloc(lag_v, new_i);
}

/* _cm_free2(): gmp-shaped free.
*/
static void
_cm_free2(void* tox_v, size_t siz_i)
{
  u3a_free(tox_v);
}

/* u3m_init(): start the environment.
*/
void
u3m_init(size_t len_i)
{
  _cm_limits();
  _cm_signals();
  _cm_crypto();

  u3a_init_once();

  //  make sure GMP uses our malloc.
  //
  mp_set_memory_functions(u3a_malloc, _cm_realloc2, _cm_free2);

  //  make sure that [len_i] is a fully-addressible non-zero power of two.
  //
  if (  !len_i
     || (len_i & (len_i - 1))
     || (len_i < (1 << (u3a_page + 2)))
     || (len_i > u3a_bytes) )
  {
    u3l_log("loom: bad size: %zu", len_i);
    exit(1);
  }

  // map at fixed address.
  //
  {
    void* map_v = mmap((void *)u3_Loom,
                       len_i,
                       (PROT_READ | PROT_WRITE),
                       (MAP_ANON | MAP_FIXED | MAP_PRIVATE),
                       -1, 0);

    if ( -1 == (c3_ps)map_v ) {
      map_v = mmap((void *)0,
                   len_i,
                   (PROT_READ | PROT_WRITE),
                   (MAP_ANON | MAP_PRIVATE),
                   -1, 0);

      u3l_log("boot: mapping %zuMB failed", len_i >> 20);
      u3l_log("see https://docs.urbit.org/user-manual/running/cloud-hosting"
              " for adding swap space");
      if ( -1 != (c3_ps)map_v ) {
        u3l_log("if porting to a new platform, try U3_OS_LoomBase %p",
                map_v);
      }
      exit(1);
    }

    u3C.wor_i = len_i >> 2;
    u3l_log("loom: mapped %zuMB", len_i >> 20);
  }
}

extern void u3je_secp_stop(void);

/* u3m_stop(): graceful shutdown cleanup.
*/
void
u3m_stop(void)
{
  u3t_sstack_exit();

  u3e_stop();
  u3je_secp_stop();
}

/* u3m_pier(): make a pier.
*/
c3_c*
u3m_pier(c3_c* dir_c)
{
  c3_c ful_c[8193];

  u3C.dir_c = dir_c;

  snprintf(ful_c, 8192, "%s", dir_c);
  if ( c3_mkdir(ful_c, 0700) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "loom: pier create: %s\r\n", strerror(errno));
      exit(1);
    }
  }

  snprintf(ful_c, 8192, "%s/.urb", dir_c);
  if ( c3_mkdir(ful_c, 0700) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "loom: .urb create: %s\r\n", strerror(errno));
      exit(1);
    }
  }

  snprintf(ful_c, 8192, "%s/.urb/chk", dir_c);
  if ( c3_mkdir(ful_c, 0700) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "loom: .urb/chk create: %s\r\n", strerror(errno));
      exit(1);
    }
  }

  return strdup(dir_c);
}

/* u3m_boot(): start the u3 system. return next event, starting from 1.
*/
c3_d
u3m_boot(c3_c* dir_c, size_t len_i)
{
  c3_o nuu_o;

  u3C.dir_c = dir_c;

  /* Activate the loom.
  */
  u3m_init(len_i);

  /* Activate the storage system.
  */
  nuu_o = u3e_live(c3n, u3m_pier(dir_c));

  /* Activate tracing.
  */
  u3C.slog_f = 0;
  u3C.sign_hold_f = 0;
  u3C.sign_move_f = 0;
  u3t_init();

  /* Construct or activate the allocator.
  */
  u3m_pave(nuu_o);

  /* GC immediately if requested
  */
  if ( (c3n == nuu_o) && (u3C.wag_w & u3o_check_corrupt) ) {
    u3l_log("boot: gc requested");
    u3m_grab(u3_none);
    u3C.wag_w &= ~u3o_check_corrupt;
    u3l_log("boot: gc complete");
  }

  /* Initialize the jet system.
  */
  {
    c3_w len_w = u3j_boot(nuu_o);
    u3l_log("boot: installed %d jets", len_w);
  }

  /* Reactivate jets on old kernel.
  */
  if ( c3n == nuu_o ) {
    u3j_ream();
    u3n_ream();
    return u3A->eve_d;
  }
  else {
  /* Basic initialization.
  */
    memset(u3A, 0, sizeof(*u3A));
    return 0;
  }
}

/* u3m_boot_lite(): start without checkpointing.
*/
c3_d
u3m_boot_lite(size_t len_i)
{
  /* Activate the loom.
  */
  u3m_init(len_i);

  /* Activate tracing.
  */
  u3C.slog_f = 0;
  u3C.sign_hold_f = 0;
  u3C.sign_move_f = 0;
  u3t_init();

  /* Construct or activate the allocator.
  */
  u3m_pave(c3y);

  /* Place the guard page.
  */
  u3e_init();

  /* Initialize the jet system.
  */
  u3j_boot(c3y);

  /* Basic initialization.
  */
  memset(u3A, 0, sizeof(*u3A));
  return 0;
}

/* u3m_reclaim: clear persistent caches to reclaim memory.
*/
void
u3m_reclaim(void)
{
  u3v_reclaim();
  u3j_reclaim();
  u3n_reclaim();
  u3a_reclaim();
}

/* _cm_pack_rewrite(): trace through arena, rewriting pointers.
*/
static void
_cm_pack_rewrite(void)
{
  //  XX fix u3a_rewrite* to support south roads
  //
  u3_assert( &(u3H->rod_u) == u3R );

  //  NB: these implementations must be kept in sync with u3m_reclaim();
  //  anything not reclaimed must be rewritable
  //
  u3v_rewrite_compact();
  u3j_rewrite_compact();
  u3n_rewrite_compact();
  u3a_rewrite_compact();
}

/* u3m_pack: compact (defragment) memory, returns u3a_open delta.
*/
c3_w
u3m_pack(void)
{
  c3_w pre_w = u3a_open(u3R);

  //  reclaim first, to free space, and discard anything we can't/don't rewrite
  //
  u3m_reclaim();

  //  sweep the heap, finding and saving new locations
  //
  u3a_pack_seek(u3R);

  //  trace roots, rewriting inner pointers
  //
  _cm_pack_rewrite();

  //  sweep the heap, relocating objects to their new locations
  //
  u3a_pack_move(u3R);

  return (u3a_open(u3R) - pre_w);
}

/* Time functions */

/* u3m_time_sec_in(): urbit seconds from unix time.
**
** Adjust for future leap secs!
*/
c3_d
u3m_time_sec_in(c3_w unx_w)
{
  return 0x8000000cce9e0d80ULL + (c3_d)unx_w;
}

/* u3m_time_sec_out(): unix time from urbit seconds.
**
** Adjust for future leap secs!
*/
c3_w
u3m_time_sec_out(c3_d urs_d)
{
  c3_d adj_d = (urs_d - 0x8000000cce9e0d80ULL);

  if ( adj_d > 0xffffffffULL ) {
    fprintf(stderr, "Agh! It's 2106! And no one's fixed this shite!\n");
    exit(1);
  }
  return (c3_w)adj_d;
}

/* u3m_time_fsc_in(): urbit fracto-seconds from unix microseconds.
*/
c3_d
u3m_time_fsc_in(c3_w usc_w)
{
  c3_d usc_d = usc_w;

  return ((usc_d * 65536ULL) / 1000000ULL) << 48ULL;
}

/* u3m_time_fsc_out: unix microseconds from urbit fracto-seconds.
*/
c3_w
u3m_time_fsc_out(c3_d ufc_d)
{
  return (c3_w) (((ufc_d >> 48ULL) * 1000000ULL) / 65536ULL);
}

/* u3m_time_msc_out: unix microseconds from urbit fracto-seconds.
*/
c3_w
u3m_time_msc_out(c3_d ufc_d)
{
  return (c3_w) (((ufc_d >> 48ULL) * 1000ULL) / 65536ULL);
}

/* u3m_time_in_tv(): urbit time from struct timeval.
*/
u3_atom
u3m_time_in_tv(struct timeval* tim_tv)
{
  c3_w unx_w = tim_tv->tv_sec;
  c3_w usc_w = tim_tv->tv_usec;
  c3_d cub_d[2];

  cub_d[0] = u3m_time_fsc_in(usc_w);
  cub_d[1] = u3m_time_sec_in(unx_w);

  return u3i_chubs(2, cub_d);
}

/* u3m_time_out_tv(): struct timeval from urbit time.
*/
void
u3m_time_out_tv(struct timeval* tim_tv, u3_noun now)
{
  c3_d ufc_d = u3r_chub(0, now);
  c3_d urs_d = u3r_chub(1, now);

  tim_tv->tv_sec = u3m_time_sec_out(urs_d);
  tim_tv->tv_usec = u3m_time_fsc_out(ufc_d);

  u3z(now);
}

/* u3m_time_in_ts(): urbit time from struct timespec.
*/
u3_atom
u3m_time_in_ts(struct timespec* tim_ts)
{
  struct timeval tim_tv;

  tim_tv.tv_sec = tim_ts->tv_sec;
  tim_tv.tv_usec = (tim_ts->tv_nsec / 1000);

  return u3m_time_in_tv(&tim_tv);
}

#if defined(U3_OS_linux) || defined(U3_OS_windows)
/* u3m_time_t_in_ts(): urbit time from time_t.
*/
u3_atom
u3m_time_t_in_ts(time_t tim)
{
  struct timeval tim_tv;

  tim_tv.tv_sec = tim;
  tim_tv.tv_usec = 0;

  return u3m_time_in_tv(&tim_tv);
}
#endif /* defined(U3_OS_linux) */

/* u3m_time_out_ts(): struct timespec from urbit time.
*/
void
u3m_time_out_ts(struct timespec* tim_ts, u3_noun now)
{
  struct timeval tim_tv;

  u3m_time_out_tv(&tim_tv, now);

  tim_ts->tv_sec = tim_tv.tv_sec;
  tim_ts->tv_nsec = (tim_tv.tv_usec * 1000);
}

/* u3m_time_out_it(): struct itimerval from urbit time gap.
** returns true if it_value is set to non-zero values, false otherwise
*/
c3_t
u3m_time_out_it(struct itimerval* tim_it, u3_noun gap)
{
  struct timeval tim_tv;
  c3_d ufc_d = u3r_chub(0, gap);
  c3_d urs_d = u3r_chub(1, gap);
  tim_it->it_value.tv_sec  = urs_d;
  tim_it->it_value.tv_usec = u3m_time_fsc_out(ufc_d);
  u3z(gap);
  return tim_it->it_value.tv_sec || tim_it->it_value.tv_usec;
}

/* u3m_time_gap_ms(): (wen - now) in ms.
*/
c3_d
u3m_time_gap_ms(u3_noun now, u3_noun wen)
{
  if ( c3n == u3ka_gth(u3k(wen), u3k(now)) ) {
    u3z(wen); u3z(now);
    return 0ULL;
  }
  else {
    u3_noun dif   = u3ka_sub(wen, now);
    c3_d    fsc_d = u3r_chub(0, dif);
    c3_d    sec_d = u3r_chub(1, dif);

    u3z(dif);
    return (sec_d * 1000ULL) + u3m_time_msc_out(fsc_d);
  }
}

/* u3m_time_gap_double(): (wen - now) in libev resolution.
*/
double
u3m_time_gap_double(u3_noun now, u3_noun wen)
{
  mpz_t now_mp, wen_mp, dif_mp;
  double sec_g = (((double)(1ULL << 32ULL)) * ((double)(1ULL << 32ULL)));
  double gap_g, dif_g;

  u3r_mp(now_mp, now);
  u3r_mp(wen_mp, wen);
  mpz_init(dif_mp);
  mpz_sub(dif_mp, wen_mp, now_mp);

  u3z(now);
  u3z(wen);

  dif_g = mpz_get_d(dif_mp) / sec_g;
  gap_g = (dif_g > 0.0) ? dif_g : 0.0;
  mpz_clear(dif_mp); mpz_clear(wen_mp); mpz_clear(now_mp);

  return gap_g;
}

/* u3m_time_gap_in_mil(): urbit time gap from milliseconds
*/
u3_atom
u3m_time_gap_in_mil(c3_w mil_w)
{
  c3_d sec_d = mil_w / 1000;
  c3_d usc_d = 1000 * (mil_w % 1000);
  c3_d cub_d[2];

  cub_d[0] = u3m_time_fsc_in(usc_d);
  cub_d[1] = sec_d;
  return u3i_chubs(2, cub_d);
}
