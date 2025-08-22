/// @file

#include "vere.h"

#include "noun.h"
#include "ur/ur.h"

#undef LORD_TRACE_JAM
#undef LORD_TRACE_CUE

/*
|%
::  peek=[gang (each path $%([%once @tas @tas path] [%beam @tas beam]))]
::  ovum=ovum
::
|$  [peek ovum]
|%
::  +task: from urth to mars
::
+$  task
  $%  [%live ?(%meld %pack) ~] :: XX rename
      [%exit ~]
      [%peek mil=@ peek]
      [%poke mil=@ ovum]
      $:  %quiz
          $%  [%quac ~]
              [%quic ~]
      ==  ==
      [%sync %save ~]
  ==
::  +gift: from mars to urth
::
+$  gifts
  $%  [%live ~]
      [%flog cord]
      [%slog pri=@ tank]
      [%peek p=(each (unit (cask)) goof)]
      [%poke p=(each (list ovum) (list goof))]
      $:  %quiz
          $%  [%quac p=*]
              [%quic p=*]
      ==  ==
      [%ripe [pro=%2 kel=wynn] [who=@p fake=?] eve=@ mug=@]
      [%sync eve=@ mug=@]
  ==
--
*/

/* _lord_stop_cb(): finally all done.
*/
static void
_lord_stop_cb(void*       ptr_v,
              ssize_t     err_i,
              const c3_c* err_c)
{
  u3_lord* god_u = ptr_v;

  void (*exit_f)(void*) = god_u->cb_u.exit_f;
  void* exit_v = god_u->cb_u.ptr_v;

  u3s_cue_xeno_done(god_u->sil_u);
  c3_free(god_u);

  if ( exit_f ) {
    exit_f(exit_v);
  }
}

/* _lord_writ_free(): dispose of pending writ.
*/
static void
_lord_writ_free(u3_writ* wit_u)
{
  switch ( wit_u->typ_e ) {
    default: break;

    case u3_writ_poke: {
      //  XX confirm
      //
      u3_ovum* egg_u = wit_u->wok_u.egg_u;
      u3_auto_drop(egg_u->car_u, egg_u);
      u3z(wit_u->wok_u.job);
    } break;

    case u3_writ_peek: {
      u3z(wit_u->pek_u->sam);
    } break;
  }

  c3_free(wit_u);
}

/* _lord_bail_noop(): ignore subprocess error on shutdown
*/
static void
_lord_bail_noop(void*       ptr_v,
                ssize_t     err_i,
                const c3_c* err_c)
{
}

/* _lord_stop(): close and dispose all resources.
*/
static void
_lord_stop(u3_lord* god_u)
{
  //  dispose outstanding writs
  //
  {
    u3_writ* wit_u = god_u->ext_u;
    u3_writ* nex_u;

    while ( wit_u ) {
      nex_u = wit_u->nex_u;
      _lord_writ_free(wit_u);
      wit_u = nex_u;
    }

    god_u->ent_u = god_u->ext_u = 0;
  }

  u3_newt_moat_stop(&god_u->out_u, _lord_stop_cb);
  u3_newt_mojo_stop(&god_u->inn_u, _lord_bail_noop);

  uv_read_stop((uv_stream_t*)&(god_u->err_u));

  uv_close((uv_handle_t*)&god_u->cub_u, 0);

#if defined(LORD_TRACE_JAM) || defined(LORD_TRACE_CUE)
  u3t_trace_close();
#endif
}

/* _lord_bail(): serf/lord error.
*/
static void
_lord_bail(u3_lord* god_u)
{
  void (*bail_f)(void*) = god_u->cb_u.bail_f;
  void* bail_v = god_u->cb_u.ptr_v;

  u3_lord_halt(god_u);
  bail_f(bail_v);
}

/* _lord_writ_str(): writ labels for printing.
*/
static inline const c3_c*
_lord_writ_str(u3_writ_type typ_e)
{
  switch ( typ_e ) {
    default: u3_assert(0);

    case u3_writ_poke: return "poke";
    case u3_writ_peek: return "peek";
    case u3_writ_live: return "live";
    case u3_writ_exit: return "exit";
    case u3_writ_quiz: return "quiz";
  }
}

/* _lord_writ_need(): require writ type.
*/
static u3_writ*
_lord_writ_need(u3_lord* god_u, u3_writ_type typ_e)
{
  u3_writ* wit_u = god_u->ext_u;

  if ( !wit_u ) {
    fprintf(stderr, "lord: missing writ, expected %%%s\r\n",
                    _lord_writ_str(wit_u->typ_e));
    _lord_bail(god_u);
    return 0;
  }

  if ( !wit_u->nex_u ) {
    god_u->ent_u = god_u->ext_u = 0;
  }
  else {
    god_u->ext_u = wit_u->nex_u;
    wit_u->nex_u = 0;
  }

  god_u->dep_w--;

  if ( typ_e != wit_u->typ_e ) {
    fprintf(stderr, "lord: unexpected %%%s, expected %%%s\r\n",
                    _lord_writ_str(typ_e),
                    _lord_writ_str(wit_u->typ_e));
    _lord_bail(god_u);
    return 0;
  }

  return wit_u;
}

/* _lord_plea_foul():
*/
static void
_lord_plea_foul(u3_lord* god_u, c3_m mot_m, u3_noun dat)
{
  if ( u3_blip == mot_m ) {
    fprintf(stderr, "lord: received invalid $plea\r\n");
  }
  else {
    fprintf(stderr, "lord: received invalid %%%.4s $plea\r\n", (c3_c*)&mot_m);
  }

  //  XX can't unconditionally print
  //
  // u3m_p("plea", dat);

  _lord_bail(god_u);
}

/* _lord_plea_live(): hear serf %live ack
*/
static void
_lord_plea_live(u3_lord* god_u, u3_noun dat)
{
  u3_writ* wit_u = _lord_writ_need(god_u, u3_writ_live);

  if( u3_nul != dat ) {
    return _lord_plea_foul(god_u, c3__live, dat);
  }

  //  XX wire into cb
  //
  // u3l_log("pier: meld complete\n");
  // u3l_log("pier: pack complete\n");

  c3_free(wit_u);
}

/* _lord_plea_ripe(): hear serf startup state
*/
static void
_lord_plea_ripe(u3_lord* god_u, u3_noun dat)
{
  if ( c3y == god_u->liv_o ) {
    fprintf(stderr, "lord: received unexpected %%ripe\n");
    _lord_bail(god_u);
    return;
  }

  {
    u3_noun a, b, c, pro, wyn, who, fak, eve, mug;
    c3_y pro_y;
    c3_d eve_d;
    c3_l mug_l;

    //  XX parse out version values
    //
    if (  (c3n == u3r_trel(dat, &a, &b, &c))
       || (c3n == u3r_cell(a, &pro, &wyn))
       || (c3n == u3r_safe_byte(pro, &pro_y))
       || (c3n == u3r_cell(b, &who, &fak))
       || (c3n == u3r_cell(c, &eve, &mug))
       || (c3n == u3r_safe_chub(eve, &eve_d))
       || (c3n == u3r_safe_word(mug, &mug_l)) )
    {
      return _lord_plea_foul(god_u, c3__ripe, dat);
    }

    if ( 2 != pro_y ) {
      fprintf(stderr, "pier: unsupported ipc protocol version %u\r\n", pro_y);
      _lord_bail(god_u);
      return;
    }

    //  XX save mug_l
    //
    god_u->eve_d = eve_d;

    god_u->liv_o = c3y;
    god_u->cb_u.live_f(god_u->cb_u.ptr_v, who, fak);
  }
;

  u3z(dat);
}

/* _lord_plea_slog(): hear serf debug output
*/
static void
_lord_plea_slog(u3_lord* god_u, u3_noun dat)
{
  u3_noun pri, tan;
  c3_w pri_w;

  if (  (c3n == u3r_cell(dat, &pri, &tan))
     || (c3n == u3r_safe_word(pri, &pri_w)) )
  {
    return _lord_plea_foul(god_u, c3__slog, dat);
  }

  god_u->cb_u.slog_f(god_u->cb_u.ptr_v, pri_w, u3k(tan));
  u3z(dat);
}

/* _lord_plea_flog(): hear serf debug output
*/
static void
_lord_plea_flog(u3_lord* god_u, u3_noun dat)
{
  u3_pier* pir_u = god_u->cb_u.ptr_v;

  if ( c3n == u3a_is_atom(dat) ) {
    return _lord_plea_foul(god_u, c3__flog, dat);
  }

  c3_c* tan_c = u3r_string(dat);
  u3C.stderr_log_f(tan_c);
  c3_free(tan_c);

  if ( 0 != pir_u->sog_f ) {
    pir_u->sog_f(pir_u->sop_p, 0, u3k(dat));
  }
  u3z(dat);
}

/* _lord_plea_peek_bail(): hear serf %peek %bail
*/
static void
_lord_plea_peek_bail(u3_lord* god_u, u3_peek* pek_u, u3_noun dud)
{
  u3_pier_punt_goof("peek", dud);

  pek_u->fun_f(pek_u->ptr_v, u3_nul);

  u3z(pek_u->sam);
  c3_free(pek_u);
}

/* _lord_plea_peek_done(): hear serf %peek %done
*/
static void
_lord_plea_peek_done(u3_lord* god_u, u3_peek* pek_u, u3_noun rep)
{
  //  XX review
  //
  if (  (u3_pico_once == pek_u->typ_e)
     && (u3_nul != rep) )
  {
    u3_noun dat;

    if ( c3y == u3r_pq(u3t(rep), c3__omen, 0, &dat) ) {
      u3k(dat);
      u3z(rep);
      rep = u3nc(u3_nul, dat);
    }
  }

  //  XX cache [dat] (unless last)
  //
  pek_u->fun_f(pek_u->ptr_v, rep);

  u3z(pek_u->sam);
  c3_free(pek_u);
}

/* _lord_plea_peek(): hear serf %peek response
*/
static void
_lord_plea_peek(u3_lord* god_u, u3_noun dat)
{
  u3_peek* pek_u;
  {
    u3_writ* wit_u = _lord_writ_need(god_u, u3_writ_peek);
    pek_u = wit_u->pek_u;
    c3_free(wit_u);
  }

  if ( c3n == u3a_is_cell(dat) ) {
    return _lord_plea_foul(god_u, c3__peek, dat);
  }

  switch ( u3h(dat) ) {
    default: {
      return _lord_plea_foul(god_u, c3__peek, dat);
    }

    case c3y: {
      _lord_plea_peek_done(god_u, pek_u, u3k(u3t(dat)));
    } break;

    case c3n: {
      _lord_plea_peek_bail(god_u, pek_u, u3k(u3t(dat)));
    } break;
  }

  u3z(dat);
}

/* _lord_plea_quiz(): handle quiz (query to serf).
 */
static void
_lord_plea_quiz(u3_lord* god_u, u3_noun dat)
{
  u3_writ* wit_u = _lord_writ_need(god_u, u3_writ_quiz);
  wit_u->qiz_u.qiz_f(wit_u->qiz_u.qiz_m, wit_u->qiz_u.ptr_v, dat);
}

/* _lord_work_spin(): update spinner if more work is in progress.
 */
 static void
_lord_work_spin(u3_lord* god_u)
{
  u3_writ* wit_u = god_u->ext_u;

  //  complete spinner
  //
  u3_assert( c3y == god_u->pin_o );
  god_u->cb_u.spun_f(god_u->cb_u.ptr_v);
  god_u->pin_o = c3n;

  //  restart spinner if more work
  //
  while ( wit_u ) {
    if ( u3_writ_poke != wit_u->typ_e ) {
      wit_u = wit_u->nex_u;
    }
    else {
      u3_ovum* egg_u = wit_u->wok_u.egg_u;

      god_u->cb_u.spin_f(god_u->cb_u.ptr_v,
                         egg_u->pin_u.lab,
                         egg_u->pin_u.del_o);
      god_u->pin_o = c3y;
      break;
    }
  }
}

/* _lord_plea_work_bail(): hear serf %work %bail
*/
static void
_lord_plea_work_bail(u3_lord* god_u, u3_ovum* egg_u, u3_noun lud)
{
  _lord_work_spin(god_u);

  god_u->cb_u.work_bail_f(god_u->cb_u.ptr_v, egg_u, lud);
}

/* _lord_plea_work_done(): hear serf %work %done
*/
static void
_lord_plea_work_done(u3_lord* god_u,
                     u3_ovum* egg_u,
                     u3_noun    act)
{
  god_u->eve_d++;

  _lord_work_spin(god_u);

  god_u->cb_u.work_done_f(god_u->cb_u.ptr_v, egg_u, act);
}

/* _lord_plea_work(): hear serf %work response
*/
static void
_lord_plea_work(u3_lord* god_u, u3_noun dat)
{
  u3_ovum* egg_u;
  u3_noun    job;

  {
    u3_writ*  wit_u = _lord_writ_need(god_u, u3_writ_poke);
    egg_u = wit_u->wok_u.egg_u;
    job   = wit_u->wok_u.job;
    c3_free(wit_u);
  }

  if ( c3n == u3a_is_cell(dat) ) {
    u3z(job);
    u3_ovum_free(egg_u);
    return _lord_plea_foul(god_u, c3__work, dat);
  }

  switch ( u3h(dat) ) {
    default: {
      u3z(job);
      u3_ovum_free(egg_u);
      return _lord_plea_foul(god_u, c3__work, dat);
    } break;

    case c3y: {
      u3z(job);
      _lord_plea_work_done(god_u, egg_u, u3k(u3t(dat)));
    } break;

    case c3n: {
      u3z(job);
      _lord_plea_work_bail(god_u, egg_u, u3k(u3t(dat)));
    } break;
  }

  u3z(dat);
}

/* _lord_on_plea(): handle plea from serf.
*/
static c3_o
_lord_on_plea(void* ptr_v, c3_d len_d, c3_y* byt_y)
{
  u3_lord* god_u = ptr_v;
  u3_noun    tag, dat;
  u3_weak    jar;

#ifdef LORD_TRACE_CUE
  u3t_event_trace("king ipc cue", 'B');
#endif

  jar = u3s_cue_xeno_with(god_u->sil_u, len_d, byt_y);

#ifdef LORD_TRACE_CUE
  u3t_event_trace("king ipc cue", 'E');
#endif

  if ( u3_none == jar ) {
    _lord_plea_foul(god_u, 0, u3_blip);
    return c3y;
  }
  else if ( c3n == u3r_cell(jar, &tag, &dat) ) {
    _lord_plea_foul(god_u, 0, jar);
    u3z(jar);
    return c3y;
  }

  switch ( tag ) {
    default: {
      _lord_plea_foul(god_u, 0, jar);
    } break;

    case c3__poke: {
      // u3m_p("poke resp jar", jar);
      _lord_plea_work(god_u, u3k(dat));
    } break;

    case c3__peek: {
      _lord_plea_peek(god_u, u3k(dat));
    } break;

    case  c3__slog: {
      _lord_plea_slog(god_u, u3k(dat));
    } break;

    case  c3__flog: {
      _lord_plea_flog(god_u, u3k(dat));
    } break;

    //  XX track snapshot state, mug_l, notification cb?
    //
    case c3__sync: {
    } break;

    case c3__live: {
      _lord_plea_live(god_u, u3k(dat));
    } break;

    case c3__ripe: {
      _lord_plea_ripe(god_u, u3k(dat));
    } break;

    case c3__quiz: {
      _lord_plea_quiz(god_u, u3k(dat));
    }
  }

  u3z(jar);
  return c3y;
}

/* _lord_writ_new(): allocate a new writ.
*/
static u3_writ*
_lord_writ_new(u3_lord* god_u)
{
  u3_writ* wit_u = c3_calloc(sizeof(*wit_u));
  if ( !god_u->ent_u ) {
    u3_assert( !god_u->ext_u );
    u3_assert( !god_u->dep_w );
    god_u->dep_w = 1;
    god_u->ent_u = god_u->ext_u = wit_u;
  }
  else {
    god_u->dep_w++;
    god_u->ent_u->nex_u = wit_u;
    god_u->ent_u = wit_u;
  }

  return wit_u;
}

/* _lord_writ_make(): cons writ.
*/
static u3_noun
_lord_writ_make(u3_lord* god_u, u3_writ* wit_u)
{
  u3_noun msg;

  switch ( wit_u->typ_e ) {
    default: u3_assert(0);

    case u3_writ_poke: {
      u3_noun mil = u3i_words(1, &wit_u->wok_u.egg_u->mil_w);
      msg = u3nt(c3__poke, mil, u3k(wit_u->wok_u.job));
    } break;

    case u3_writ_peek: {
      //  XX support timeouts,
      //
      msg = u3nc(c3__peek, u3nc(0, u3k(wit_u->pek_u->sam)));
    } break;

    //  XX wat do
    //
    case u3_writ_live: {
      msg = u3_nul;
    } break;

    case u3_writ_quiz: {
      msg = u3nt(c3__quiz, wit_u->qiz_u.qiz_m, u3_nul);
    }

    case u3_writ_exit: {
      msg = u3nc(c3__exit, u3_nul);
    } break;
  }

  return msg;
}

/* _lord_writ_send(): send writ to serf.
*/
static void
_lord_writ_send(u3_lord* god_u, u3_writ* wit_u)
{
  //  exit expected
  //
  if ( u3_writ_exit == wit_u->typ_e ) {
    god_u->out_u.bal_f = _lord_bail_noop;
    god_u->inn_u.bal_f = _lord_bail_noop;
  }

  {
    u3_noun jar = _lord_writ_make(god_u, wit_u);
    c3_d  len_d;
    c3_y* byt_y;

#ifdef LORD_TRACE_JAM
    u3t_event_trace("king ipc jam", 'B');
#endif

    u3s_jam_xeno(jar, &len_d, &byt_y);

#ifdef LORD_TRACE_JAM
    u3t_event_trace("king ipc jam", 'E');
#endif

    u3_newt_send(&god_u->inn_u, len_d, byt_y);
    u3z(jar);
  }
}

/* _lord_send(): send writ to serf.
*/
static void
_lord_send(u3_lord* god_u, u3_noun jar)
{
  c3_d  len_d;
  c3_y* byt_y;

#ifdef LORD_TRACE_JAM
  u3t_event_trace("king ipc jam", 'B');
#endif

  u3s_jam_xeno(jar, &len_d, &byt_y);

#ifdef LORD_TRACE_JAM
  u3t_event_trace("king ipc jam", 'E');
#endif

  u3_newt_send(&god_u->inn_u, len_d, byt_y);
  u3z(jar);
}

/* u3_lord_peek(): read namespace, injecting what's missing.
*/
void
u3_lord_peek(u3_lord* god_u, u3_pico* pic_u)
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_peek;
  wit_u->pek_u = c3_calloc(sizeof(*wit_u->pek_u));
  wit_u->pek_u->ptr_v = pic_u->ptr_v;
  wit_u->pek_u->fun_f = pic_u->fun_f;
  wit_u->pek_u->typ_e = pic_u->typ_e;

  //  construct the full scry path
  //
  {
    u3_noun sam;
    switch ( pic_u->typ_e ) {
      default: u3_assert(0);

      case u3_pico_full: {
        sam = u3k(pic_u->ful);
      } break;

      case u3_pico_once: {
        sam = u3nc(c3n, u3nq(c3__once,
                             pic_u->las_u.car_m,
                             u3k(pic_u->las_u.des),
                             u3k(pic_u->las_u.pax)));
      } break;
    }

    wit_u->pek_u->sam = u3nc(u3k(pic_u->gan), sam);
  }

  //  XX cache check, unless last
  //
  _lord_send(god_u, _lord_writ_make(god_u, wit_u));
}

/* u3_lord_work(): attempt work.
*/
void
u3_lord_work(u3_lord* god_u, u3_ovum* egg_u, u3_noun job)
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_poke;
  wit_u->wok_u.egg_u = egg_u;
  wit_u->wok_u.job = job;

  //  if not spinning, start
  //
  if ( c3n == god_u->pin_o ) {
    god_u->cb_u.spin_f(god_u->cb_u.ptr_v,
                       egg_u->pin_u.lab,
                       egg_u->pin_u.del_o);
    god_u->pin_o = c3y;
  }

  _lord_send(god_u, _lord_writ_make(god_u, wit_u));
}

/* u3_lord_save(): save a snapshot.
*/
c3_o
u3_lord_save(u3_lord* god_u)
{
  _lord_send(god_u, u3nt(c3__sync, c3__save, u3_nul));
  return c3y;
}

/* u3_lord_meld(): globally deduplicate persistent state.
*/
void
u3_lord_meld(u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_live;

  //  XX set callback
  //
  _lord_send(god_u, u3nt(c3__live, c3__meld, u3_nul));
}

/* u3_lord_pack(): defragment persistent state.
*/
void
u3_lord_pack(u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_live;

  //  XX set callback
  //
  _lord_send(god_u, u3nt(c3__live, c3__pack, u3_nul));
}

/* u3_lord_quiz(): query the serf.
*/
void
u3_lord_quiz(u3_lord* god_u,
             c3_m     qiz_m,
             void*    ptr_v,
             void (*qiz_f)(c3_m, void*, u3_noun))
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_quiz;
  wit_u->qiz_u.qiz_m = qiz_m;
  wit_u->qiz_u.ptr_v = ptr_v;
  wit_u->qiz_u.qiz_f = qiz_f;

  _lord_send(god_u, u3nt(c3__quiz, qiz_m, u3_nul));
}

/* u3_lord_exit(): shutdown gracefully.
*/
void
u3_lord_exit(u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_new(god_u);
  wit_u->typ_e = u3_writ_exit;
  _lord_send(god_u, _lord_writ_make(god_u, wit_u));

  //  exit expected
  //
  god_u->out_u.bal_f = _lord_bail_noop;
  god_u->inn_u.bal_f = _lord_bail_noop;

  //  XX set timer, then halt
  //
}

/* u3_lord_stall(): send SIGINT
*/
void
u3_lord_stall(u3_lord* god_u)
{
  uv_process_kill(&god_u->cub_u, SIGINT);
}

/* u3_lord_halt(): shutdown immediately
*/
void
u3_lord_halt(u3_lord* god_u)
{
  //  no exit callback on halt
  //
  god_u->cb_u.exit_f = 0;

  uv_process_kill(&god_u->cub_u, SIGKILL);
  _lord_stop(god_u);
}

/* _lord_serf_err_alloc(): libuv buffer allocator.
*/
static void
_lord_serf_err_alloc(uv_handle_t* had_u, size_t len_i, uv_buf_t* buf)
{
  //  error/info messages as a rule don't exceed one line
  //
  *buf = uv_buf_init(c3_malloc(80), 80);
}

/* _lord_on_serf_err_cb(): subprocess stderr callback.
*/
static void
_lord_on_serf_err_cb(uv_stream_t* pyp_u,
              ssize_t             siz_i,
              const uv_buf_t*     buf_u)
{
  if ( siz_i >= 0 ) {
    //  serf used to write to 2 directly
    //  this can't be any worse than that
    //
    u3_write_fd(2, buf_u->base, siz_i);
  } else {
    uv_read_stop(pyp_u);

    if ( siz_i != UV_EOF ) {
      u3l_log("lord: serf stderr: %s", uv_strerror(siz_i));
    }
  }

  if ( buf_u->base != NULL ) {
    c3_free(buf_u->base);
  }
}


/* _lord_on_serf_exit(): handle subprocess exit.
*/
static void
_lord_on_serf_exit(uv_process_t* req_u,
                   c3_ds         sas_i,
                   c3_i          sig_i)
{

  u3_lord* god_u = (void*)req_u;

  if (  !god_u->ext_u
     || !(u3_writ_exit == god_u->ext_u->typ_e) )
  {
    fprintf(stderr, "pier: work exit: status %" PRId64 ", signal %d\r\n",
                  sas_i, sig_i);
    _lord_bail(god_u);
  }
  else {
    _lord_stop(god_u);
  }
}

/* _lord_on_serf_bail(): handle subprocess error.
*/
static void
_lord_on_serf_bail(void*       ptr_v,
                   ssize_t     err_i,
                   const c3_c* err_c)
{
  u3_lord* god_u = ptr_v;

  if ( UV_EOF == err_i ) {
    u3l_log("pier: serf unexpectedly shut down\r\n");
  }
  else {
    u3l_log("pier: serf error: %s", err_c);
  }

  _lord_bail(god_u);
}

/* u3_lord_info(): status info as $mass.
*/
u3_noun
u3_lord_info(u3_lord* god_u)
{
  return u3_pier_mass(
    c3__lord,
    u3i_list(
      u3_pier_mase("live",  god_u->liv_o),
      u3_pier_mase("event", u3i_chub(god_u->eve_d)),
      u3_pier_mase("queue", u3i_word(god_u->dep_w)),
      u3_newt_moat_info(&god_u->out_u),
      u3_none));
}

/* u3_lord_slog(): print status info.
*/
void
u3_lord_slog(u3_lord* god_u)
{
  u3l_log("  lord: live=%s, event=%" PRIu64 ", queue=%u",
          ( c3y == god_u->liv_o ) ? "&" : "|",
          god_u->eve_d,
          god_u->dep_w);
  u3_newt_moat_slog(&god_u->out_u);
}

/* u3_lord_init(): instantiate child process.
*/
u3_lord*
u3_lord_init(c3_c* pax_c, c3_w wag_w, c3_d key_d[4], u3_lord_cb cb_u)
{
  u3_lord* god_u = c3_calloc(sizeof *god_u);
  god_u->liv_o = c3n;
  god_u->pin_o = c3n;
  god_u->wag_w = wag_w;
  god_u->bin_c = u3_Host.wrk_c; //  XX strcopy
  god_u->pax_c = pax_c;  //  XX strcopy
  god_u->cb_u  = cb_u;

  god_u->key_d[0] = key_d[0];
  god_u->key_d[1] = key_d[1];
  god_u->key_d[2] = key_d[2];
  god_u->key_d[3] = key_d[3];

  //  spawn new process and connect to it
  //
  {
    c3_c* arg_c[13];
    c3_c  key_c[256];
    c3_c  wag_c[11];
    c3_c  hap_c[11];
    c3_c  per_c[11];
    c3_c  lom_c[11];
    c3_c  tos_c[11];
    c3_c  eve_c[11];
    c3_c  cev_c[11];
    c3_i  err_i;

    sprintf(key_c, "%" PRIx64 ":%" PRIx64 ":%" PRIx64 ":%" PRIx64,
                   god_u->key_d[0],
                   god_u->key_d[1],
                   god_u->key_d[2],
                   god_u->key_d[3]);

    sprintf(wag_c, "%u", god_u->wag_w);

    sprintf(hap_c, "%u", u3_Host.ops_u.hap_w);

    sprintf(per_c, "%u", u3_Host.ops_u.per_w);

    sprintf(lom_c, "%u", u3_Host.ops_u.lom_y);

    sprintf(tos_c, "%u", u3C.tos_w);

    arg_c[0] = god_u->bin_c;            //  executable
    arg_c[1] = "work";                  //  protocol
    arg_c[2] = god_u->pax_c;            //  path to checkpoint directory
    arg_c[3] = key_c;                   //  disk key
    arg_c[4] = wag_c;                   //  runtime config
    arg_c[5] = hap_c;                   //  hash table size
    arg_c[6] = lom_c;                   //  loom bex

    if ( u3_Host.ops_u.til_c ) {
      //  XX validate
      //
      arg_c[7] = u3_Host.ops_u.til_c;
    }
    else {
      arg_c[7] = "0";
    }

    if ( u3C.eph_c == 0 ) {
      arg_c[8] = "0";
    }
    else {
      arg_c[8] = strdup(u3C.eph_c);     //  ephemeral file
    }

    arg_c[9] = tos_c;
    arg_c[10] = per_c;

#ifdef U3_OS_windows
    sprintf(cev_c, "%"PRIu64, (c3_d)u3_Host.cev_u);
    arg_c[11] = cev_c;
#else
    arg_c[11] = 0;
#endif

    arg_c[12] = NULL;

    uv_pipe_init(u3L, &god_u->inn_u.pyp_u, 0);
    uv_timer_init(u3L, &god_u->out_u.tim_u);
    uv_pipe_init(u3L, &god_u->out_u.pyp_u, 0);
    uv_pipe_init(u3L, &god_u->err_u, 0);

    god_u->cod_u[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    god_u->cod_u[0].data.stream = (uv_stream_t *)&god_u->inn_u;

    god_u->cod_u[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    god_u->cod_u[1].data.stream = (uv_stream_t *)&god_u->out_u;

    god_u->cod_u[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    god_u->cod_u[2].data.stream = (uv_stream_t *)&god_u->err_u;

    god_u->ops_u.stdio = god_u->cod_u;
    god_u->ops_u.stdio_count = 3;

    // if any fds are inherited, libuv ignores UV_PROCESS_WINDOWS_HIDE*
    god_u->ops_u.flags = UV_PROCESS_WINDOWS_HIDE;
    god_u->ops_u.exit_cb = _lord_on_serf_exit;
    god_u->ops_u.file = arg_c[0];
    god_u->ops_u.args = arg_c;

    if ( (err_i = uv_spawn(u3L, &god_u->cub_u, &god_u->ops_u)) ) {
      fprintf(stderr, "spawn: %s: %s\r\n", arg_c[0], uv_strerror(err_i));

      return 0;
    }

    uv_read_start((uv_stream_t *)&god_u->err_u, _lord_serf_err_alloc, _lord_on_serf_err_cb);
  }

#if defined(LORD_TRACE_JAM) || defined(LORD_TRACE_CUE)
  u3t_trace_open(god_u->pax_c);
#endif

  {
    god_u->sil_u = u3s_cue_xeno_init();
  }

  //  start reading from proc
  //
  {
    god_u->out_u.ptr_v = god_u;
    god_u->out_u.pok_f = _lord_on_plea;
    god_u->out_u.bal_f = _lord_on_serf_bail;

    //  XX distinguish from out_u.bal_f ?
    //
    god_u->inn_u.ptr_v = god_u;
    god_u->inn_u.bal_f = _lord_on_serf_bail;

    u3_newt_read(&god_u->out_u);
  }
  return god_u;
}

typedef struct _lord_boot {
  uv_process_t         cub_u;           //  process handle
  uv_process_options_t ops_u;           //  process configuration
  uv_stdio_container_t cod_u[3];        //  process options
  u3_cue_xeno*         sil_u;           //  cue handle
  u3_mojo              inn_u;           //  client's stdin
  u3_moat              out_u;           //  client's stdout
  c3_w                 wag_w;           //  config flags
  c3_c*                bin_c;           //  binary path
  c3_c*                pax_c;           //  directory
  c3_d                 key_d[4];        //  image key
  void*                ptr_v;
  c3_o                 ret_o;
  void (*done_f)(void*, c3_o);
} _lord_boot;

/* _lord_on_serf_boot_exit_cb(): finish handling subprocess exit.
*/
static void
_lord_on_serf_boot_exit_cb(void*       ptr_v,
                           ssize_t     err_i,
                           const c3_c* err_c)
{
  _lord_boot* bot_u               = ptr_v;
  c3_o        ret_o               = bot_u->ret_o;
  void     (*done_f)(void*, c3_o) = bot_u->done_f;

  ptr_v = bot_u->ptr_v;

  u3s_cue_xeno_done(bot_u->sil_u);
  c3_free(bot_u);

  if ( done_f ) {
    done_f(ptr_v, ret_o);
  }
}

/* _lord_on_serf_boot_exit(): handle subprocess exit.
*/
static void
_lord_on_serf_boot_exit(uv_process_t* cub_u,
                        c3_ds         sas_i,
                        c3_i          sig_i)
{
  _lord_boot* bot_u = cub_u->data;

  bot_u->ret_o = ( !sas_i && !sig_i ) ? c3y : c3n;

  u3_newt_moat_stop(&bot_u->out_u, _lord_on_serf_boot_exit_cb);
  u3_newt_mojo_stop(&bot_u->inn_u, _lord_bail_noop);

  //  XX add stderr?
  //
  uv_close((uv_handle_t*)cub_u, 0);
}

/* _lord_on_serf_boot_bail(): handle subprocess error.
*/
static void
_lord_on_serf_boot_bail(void*       ptr_v,
                        ssize_t     err_i,
                        const c3_c* err_c)
{
  _lord_boot* bot_u = ptr_v;

  // ignore EOF, dispatch on process exit
  //
  if ( UV_EOF != err_i ) {
    u3l_log("boot: error: %s\r\n", err_c);
  }
}

/* _lord_on_plea_boot(): handle plea from serf.
*/
static c3_o
_lord_on_plea_boot(void* ptr_v, c3_d len_d, c3_y* byt_y)
{
  _lord_boot* bot_u = ptr_v;

  u3_weak jar = u3s_cue_xeno_with(bot_u->sil_u, len_d, byt_y);
  u3_noun tag, dat;

  if ( u3_none == jar ) {
    //  XX fatal error
    u3_assert(0);
    // return _lord_plea_foul(god_u, 0, u3_blip);
  }
  else if ( c3n == u3r_cell(jar, &tag, &dat) ) {
    //  XX fatal error
    u3_assert(0);
    // return _lord_plea_foul(god_u, 0, jar);
  }
  else {
    switch ( tag ) {
      default: {
        //  XX fatal error
        u3_assert(0);
        // return _lord_plea_foul(god_u, 0, jar);ss
      }

      case c3__slog: {
        u3_noun pri, tan;
        c3_w pri_w;

        if (  (c3n == u3r_cell(dat, &pri, &tan))
           || (c3n == u3r_safe_word(pri, &pri_w)) )
        {
          //  XX fatal error
          u3_assert(0);
          // return _lord_plea_foul(god_u, c3__slog, dat);
        }
        else {
          u3_pier_tank(0, pri_w, u3k(tan));
        }
      } break;

      case c3__flog: {
        c3_c* tan_c = u3r_string(dat);
        u3C.stderr_log_f(tan_c);
        c3_free(tan_c);
      } break;
    }
  }

  u3z(jar);
  return c3y;
}

/* u3_lord_boot(): instantiate child process.
*/
void
u3_lord_boot(c3_c* pax_c,
             c3_w  wag_w,
             c3_d  key_d[4],
             u3_noun msg,
             void* ptr_v,
             void (*done_f)(void*, c3_o))
{
  _lord_boot* bot_u = c3_calloc(sizeof(*bot_u));
  bot_u->wag_w = wag_w;
  bot_u->bin_c = u3_Host.wrk_c; //  XX strcopy
  bot_u->pax_c = pax_c;  //  XX strcopy
  bot_u->done_f = done_f;
  bot_u->ptr_v = ptr_v;

  bot_u->key_d[0] = key_d[0];
  bot_u->key_d[1] = key_d[1];
  bot_u->key_d[2] = key_d[2];
  bot_u->key_d[3] = key_d[3];

  //  spawn new process and connect to it
  //
  {
    c3_c* arg_c[12];
    c3_c  key_c[256];
    c3_c  wag_c[11];
    c3_c  hap_c[11];
    c3_c  lom_c[11];
    c3_c  eph_c[11];
    c3_c  tos_c[11];
    c3_c  per_c[11];
    c3_i  err_i;

    sprintf(key_c, "%" PRIx64 ":%" PRIx64 ":%" PRIx64 ":%" PRIx64 "",
                   bot_u->key_d[0],
                   bot_u->key_d[1],
                   bot_u->key_d[2],
                   bot_u->key_d[3]);

    sprintf(wag_c, "%u", bot_u->wag_w);

    sprintf(hap_c, "%u", u3_Host.ops_u.hap_w);

    sprintf(lom_c, "%u", u3_Host.ops_u.lom_y);

    sprintf(tos_c, "%u", u3C.tos_w);

    sprintf(per_c, "%u", u3_Host.ops_u.per_w);

    arg_c[0] = bot_u->bin_c;            //  executable
    arg_c[1] = "boot";                  //  protocol
    arg_c[2] = bot_u->pax_c;            //  path to checkpoint directory
    arg_c[3] = key_c;                   //  disk key
    arg_c[4] = wag_c;                   //  runtime config
    arg_c[5] = hap_c;                   //  hash table size
    arg_c[6] = lom_c;                   //  loom bex

    if ( u3C.eph_c == 0 ) {
      arg_c[7] = "0";
    }
    else {
      arg_c[7] = strdup(u3C.eph_c);     //  ephemeral file
    }

    arg_c[8] = tos_c;
    arg_c[9] = per_c;
    arg_c[10] = NULL;

    uv_pipe_init(u3L, &bot_u->inn_u.pyp_u, 0);
    uv_timer_init(u3L, &bot_u->out_u.tim_u);
    uv_pipe_init(u3L, &bot_u->out_u.pyp_u, 0);

    //  XX add stderr?
    //

    bot_u->cod_u[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    bot_u->cod_u[0].data.stream = (uv_stream_t *)&bot_u->inn_u;

    bot_u->cod_u[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    bot_u->cod_u[1].data.stream = (uv_stream_t *)&bot_u->out_u;

    bot_u->cod_u[2].flags = UV_INHERIT_FD;
    bot_u->cod_u[2].data.fd = 2;

    bot_u->ops_u.stdio = bot_u->cod_u;
    bot_u->ops_u.stdio_count = 3;

    bot_u->ops_u.exit_cb = _lord_on_serf_boot_exit;
    bot_u->ops_u.file = arg_c[0];
    bot_u->ops_u.args = arg_c;

    if ( (err_i = uv_spawn(u3L, &bot_u->cub_u, &bot_u->ops_u)) ) {
      fprintf(stderr, "spawn: %s: %s\r\n", arg_c[0], uv_strerror(err_i));
      //  XX return error code, or defer and invoke cb
      //
      return;
    }
  }

  bot_u->cub_u.data = bot_u;

  {
    bot_u->sil_u = u3s_cue_xeno_init();
  }

  //  start reading from proc
  //
  {
    bot_u->out_u.ptr_v = bot_u;
    bot_u->out_u.pok_f = _lord_on_plea_boot;
    bot_u->out_u.bal_f = _lord_on_serf_boot_bail;

    //  XX distinguish from out_u.bal_f ?
    //
    bot_u->inn_u.ptr_v = bot_u;
    bot_u->inn_u.bal_f = _lord_on_serf_boot_bail;

    u3_newt_read(&bot_u->out_u);
  }

  {
    c3_d  len_d;
    c3_y* byt_y;
    u3s_jam_xeno(msg, &len_d, &byt_y);
    u3_newt_send(&bot_u->inn_u, len_d, byt_y);
    u3z(msg);
  }
}
