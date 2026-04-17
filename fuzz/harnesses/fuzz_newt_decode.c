/// @file fuzz_newt_decode.c
///
/// H6 — AFL++ harness for u3_newt_decode, the king↔serf IPC framing
/// protocol. Length-prefixed (5-byte header) blob messages.
///
/// We construct a u3_moat by zero-initialising it and feed it
/// fuzz-controlled chunks via u3_newt_decode. The decoder's state
/// machine is what we're stressing — header parsing, length math,
/// body collection, message queue management.
///
/// On a complete message, _newt_meat_plan calls uv_timer_start. We
/// initialize a libuv loop just enough to make that call legal; we
/// never run the loop.
///
/// **Throughput TODO**: H6 runs at ~38 execs/sec, vs ~1200 for H4
/// (also fork mode, also ASan-instrumented, similar binary size).
/// The per-iteration cost is dominated by something AFL-side that's
/// specific to this binary — pre-initing libuv outside __AFL_INIT
/// didn't help, and a direct shell-invoked single run takes only
/// ~9 ms. Worth investigating before doing long campaigns; maybe a
/// timeout / forkserver tuning issue.
///
/// Because newt.c lives in pkg/vere and pulls in the whole noun
/// kitchen sink at compile time, we compile newt.c into the harness
/// and rely on -Wl,--gc-sections to drop unused functions. Anything
/// the gc still pulls in (u3_pier_mase, u3l_log, etc.) is resolved
/// from libnoun.a or our own stubs.
///
/// Build:   fuzz/build.sh fuzz_newt_decode
/// Corpus:  fuzz/corpus/fuzz_newt_decode/
/// Dict:    fuzz/dicts/newt.dict

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Pull in vere.h (which pulls noun.h, uv.h, lmdb headers) before
 * declaring main. */
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static u3_moat g_moat;
static uv_loop_t g_loop;

/* Required by newt.c when it logs from the moat status path; we
 * don't exercise that path but the linker still wants the symbol
 * resolved. Provide a no-op stub so we don't need to drag in pkg/vere
 * pier code. */
static c3_o
_fuzz_pok(void* ptr_v, c3_d len_d, c3_y* byt_y)
{
  (void)ptr_v; (void)len_d; (void)byt_y;
  return c3y;
}

static void
_fuzz_bal(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  (void)ptr_v; (void)err_i; (void)err_c;
}

/* Stubs for pkg/vere/pier symbols that newt.c's u3_newt_moat_info
 * references but that we don't reach during fuzzing. The linker keeps
 * u3_newt_moat_info around (it has external linkage) so these symbols
 * must resolve, but they're never called from main(). */
u3_noun
u3_pier_mase(c3_c* cod_c, u3_noun dat)
{
  (void)cod_c; (void)dat;
  return u3_nul;
}

u3_noun
u3_pier_mass(u3_atom cod, u3_noun lit)
{
  (void)cod; (void)lit;
  return u3_nul;
}

int
main(void)
{
  /* The noun runtime is required because vere.h includes noun.h and
   * c3_malloc/c3_free are part of pkg/c3 which expects loom presence
   * in some paths. 16 MiB matches the other harnesses. */
  u3m_boot_lite(1 << 24);

  /* Bring up libuv enough that uv_timer_init / uv_timer_start succeed
   * (newt.c's _newt_meat_plan calls uv_timer_start when a complete
   * message is decoded). We never call uv_run so timers don't fire. */
  uv_loop_init(&g_loop);

  /* Initialize libuv handles ONCE before the forkserver snapshot.
   * uv_timer_init / uv_pipe_init each take ~10ms because they
   * allocate page-aligned internal buffers; per-iteration init was
   * the dominant cost. Forked children inherit the initialized
   * handles from the snapshot.
   *
   * The decoder only writes to the parser-state fields of u3_moat
   * (mes_u, ent_u, ext_u). It calls uv_timer_start through the
   * already-initialized tim_u handle. We reset the parser-state
   * fields on each iteration without touching the libuv handles. */
  uv_pipe_init(&g_loop, &g_moat.pyp_u, 0);
  uv_timer_init(&g_loop, &g_moat.tim_u);
  g_moat.pok_f = _fuzz_pok;
  g_moat.bal_f = _fuzz_bal;

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

  while (__AFL_LOOP(1000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    if (len < 1 || len > H_MAX_INPUT) continue;

    /* Reset parser state but keep libuv handles intact. */
    memset(&g_moat.mes_u, 0, sizeof(g_moat.mes_u));
    g_moat.mes_u.sat_e = u3_mess_head;
    g_moat.ent_u = NULL;
    g_moat.ext_u = NULL;

    (void)u3_newt_decode(&g_moat, (c3_y*)buf, (c3_d)len);

    /* Free any meat structs the decoder enqueued. */
    u3_meat* met = g_moat.ext_u;
    while (met) {
      u3_meat* nxt = met->nex_u;
      c3_free(met);
      met = nxt;
    }
  }

  return 0;
}
