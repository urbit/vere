/// @file

#include "vere.h"

#include "noun.h"
#include "ur/ur.h"

#undef LORD_TRACE_JAM
#undef LORD_TRACE_CUE

/*
|%
::  +writ: from king to serf
::
+$  writ
  $%  $:  %live
          $%  [%cram eve=@]
              [%exit cod=@]
              [%save eve=@]
              [%meld ~]
              [%pack ~]
      ==  ==
      [%peek mil=@ sam=*]  :: gang c3_w_tmp
each path $%c3_w_tmp
[%once @tas @tas path] [%beam @tas beam]))
      [%play eve=@ lit=c3_w_tmp
list ?c3_w_tmp
c3_w_tmp
pair @da ovum) *))]
      [%work mil=@ job=c3_w_tmp
pair @da ovum)]
  ==
::  +plea: from serf to king
::
+$  plea
  $%  [%live ~]
      [%ripe [pro=%1 hon=@ nok=@] eve=@ mug=@]
      [%slog pri=@ tank]
      [%flog cord]
      $:  %peek
          $%  [%done dat=c3_w_tmp
unit c3_w_tmp
cask))]
              [%bail dud=goof]
      ==  ==
      $:  %play
          $%  [%done mug=@]
              [%bail eve=@ mug=@ dud=goof]
      ==  ==
      $:  %work
          $%  [%done eve=@ mug=@ fec=c3_w_tmp
list ovum)]
              [%swap eve=@ mug=@ job=c3_w_tmp
pair @da ovum) fec=c3_w_tmp
list ovum)]
              [%bail lud=c3_w_tmp
list goof)]
      ==  ==
  ==
--
*/

/* _lord_stop_cbc3_w_tmp
): finally all done.
*/
static void
_lord_stop_cbc3_w_tmp
void*       ptr_v,
              ssize_t     err_i,
              const c3_c* err_c)
{
  u3_lord* god_u = ptr_v;

  void c3_w_tmp
*exit_f)c3_w_tmp
void*) = god_u->cb_u.exit_f;
  void* exit_v = god_u->cb_u.ptr_v;

  u3s_cue_xeno_donec3_w_tmp
god_u->sil_u);
  c3_freec3_w_tmp
god_u);

  if c3_w_tmp
 exit_f ) {
    exit_fc3_w_tmp
exit_v);
  }
}

/* _lord_writ_freec3_w_tmp
): dispose of pending writ.
*/
static void
_lord_writ_freec3_w_tmp
u3_writ* wit_u)
{
  switch c3_w_tmp
 wit_u->typ_e ) {
    default: u3_assertc3_w_tmp
0);

    case u3_writ_work: {
      //  XX confirm
      //
      u3_ovum* egg_u = wit_u->wok_u.egg_u;
      u3_auto_dropc3_w_tmp
egg_u->car_u, egg_u);
      u3zc3_w_tmp
wit_u->wok_u.job);
    } break;

    case u3_writ_peek: {
      u3zc3_w_tmp
wit_u->pek_u->sam);
    } break;

    case u3_writ_play: {
      u3_fact* tac_u = wit_u->fon_u.ext_u;
      u3_fact* nex_u;

      while c3_w_tmp
 tac_u ) {
        nex_u = tac_u->nex_u;
        u3_fact_freec3_w_tmp
tac_u);
        tac_u = nex_u;
      }
    } break;

    case u3_writ_save:
    case u3_writ_cram:
    case u3_writ_meld:
    case u3_writ_pack:
    case u3_writ_exit: {
    } break;
  }

  c3_freec3_w_tmp
wit_u);
}

/* _lord_bail_noopc3_w_tmp
): ignore subprocess error on shutdown
*/
static void
_lord_bail_noopc3_w_tmp
void*       ptr_v,
                ssize_t     err_i,
                const c3_c* err_c)
{
}

/* _lord_stopc3_w_tmp
): close and dispose all resources.
*/
static void
_lord_stopc3_w_tmp
u3_lord* god_u)
{
  //  dispose outstanding writs
  //
  {
    u3_writ* wit_u = god_u->ext_u;
    u3_writ* nex_u;

    while c3_w_tmp
 wit_u ) {
      nex_u = wit_u->nex_u;
      _lord_writ_freec3_w_tmp
wit_u);
      wit_u = nex_u;
    }

    god_u->ent_u = god_u->ext_u = 0;
  }

  u3_newt_moat_stopc3_w_tmp
&god_u->out_u, _lord_stop_cb);
  u3_newt_mojo_stopc3_w_tmp
&god_u->inn_u, _lord_bail_noop);

  uv_read_stopc3_w_tmp
c3_w_tmp
uv_stream_t*)&c3_w_tmp
god_u->err_u));

  uv_closec3_w_tmp
c3_w_tmp
uv_handle_t*)&god_u->cub_u, 0);

#if definedc3_w_tmp
LORD_TRACE_JAM) || definedc3_w_tmp
LORD_TRACE_CUE)
  u3t_trace_closec3_w_tmp
);
#endif
}

/* _lord_bailc3_w_tmp
): serf/lord error.
*/
static void
_lord_bailc3_w_tmp
u3_lord* god_u)
{
  void c3_w_tmp
*bail_f)c3_w_tmp
void*) = god_u->cb_u.bail_f;
  void* bail_v = god_u->cb_u.ptr_v;

  u3_lord_haltc3_w_tmp
god_u);
  bail_fc3_w_tmp
bail_v);
}

/* _lord_writ_popc3_w_tmp
): pop the writ stack.
*/
static u3_writ*
_lord_writ_popc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = god_u->ext_u;

  u3_assertc3_w_tmp
 wit_u );

  if c3_w_tmp
 !wit_u->nex_u ) {
    god_u->ent_u = god_u->ext_u = 0;
  }
  else {
    god_u->ext_u = wit_u->nex_u;
    wit_u->nex_u = 0;
  }

  god_u->dep_w--;

  return wit_u;
}

/* _lord_writ_strc3_w_tmp
): writ labels for printing.
*/
static inline const c3_c*
_lord_writ_strc3_w_tmp
u3_writ_type typ_e)
{
  switch c3_w_tmp
 typ_e ) {
    default: u3_assertc3_w_tmp
0);

    case u3_writ_work: return "work";
    case u3_writ_peek: return "peek";
    case u3_writ_play: return "play";
    case u3_writ_save: return "save";
    case u3_writ_cram: return "cram";
    case u3_writ_meld: return "meld";
    case u3_writ_pack: return "pack";
    case u3_writ_exit: return "exit";
  }
}

/* _lord_writ_needc3_w_tmp
): require writ type.
*/
static u3_writ*
_lord_writ_needc3_w_tmp
u3_lord* god_u, u3_writ_type typ_e)
{
  u3_writ* wit_u = _lord_writ_popc3_w_tmp
god_u);

  if c3_w_tmp
 typ_e != wit_u->typ_e ) {
    fprintfc3_w_tmp
stderr, "lord: unexpected %%%s, expected %%%s\r\n",
                    _lord_writ_strc3_w_tmp
typ_e),
                    _lord_writ_strc3_w_tmp
wit_u->typ_e));
    _lord_bailc3_w_tmp
god_u);
    return 0;
  }

  return wit_u;
}

/* _lord_plea_foulc3_w_tmp
):
*/
static void
_lord_plea_foulc3_w_tmp
u3_lord* god_u, c3_m mot_m, u3_noun dat)
{
  if c3_w_tmp
 u3_blip == mot_m ) {
    fprintfc3_w_tmp
stderr, "lord: received invalid $plea\r\n");
  }
  else {
    fprintfc3_w_tmp
stderr, "lord: received invalid %%%.4s $plea\r\n", c3_w_tmp
c3_c*)&mot_m);
  }

  //  XX can't unconditionally print
  //
  // u3m_pc3_w_tmp
"plea", dat);

  _lord_bailc3_w_tmp
god_u);
}

/* _lord_plea_livec3_w_tmp
): hear serf %live ack
*/
static void
_lord_plea_livec3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_writ* wit_u = _lord_writ_popc3_w_tmp
god_u);

  ifc3_w_tmp
 u3_nul != dat ) {
    _lord_plea_foulc3_w_tmp
god_u, c3__live, dat);
    return;
  }

  switch c3_w_tmp
 wit_u->typ_e ) {
    default: {
      _lord_plea_foulc3_w_tmp
god_u, c3__live, dat);
      return;
    } break;

    case u3_writ_save: {
      god_u->cb_u.save_fc3_w_tmp
god_u->cb_u.ptr_v);
    } break;

    case u3_writ_cram: {
      god_u->cb_u.cram_fc3_w_tmp
god_u->cb_u.ptr_v);
    } break;

    case u3_writ_meld: {
      //  XX wire into cb
      //
      u3l_logc3_w_tmp
"pier: meld complete");
    } break;

    case u3_writ_pack: {
      //  XX wire into cb
      //
      u3l_logc3_w_tmp
"pier: pack complete");
    } break;
  }

  c3_freec3_w_tmp
wit_u);
}

/* _lord_plea_ripec3_w_tmp
): hear serf startup state
*/
static void
_lord_plea_ripec3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  if c3_w_tmp
 c3y == god_u->liv_o ) {
    fprintfc3_w_tmp
stderr, "lord: received unexpected %%ripe\n");
    _lord_bailc3_w_tmp
god_u);
    return;
  }

  {
    u3_noun ver, pro, hon, noc, eve, mug;
    c3_y pro_y, hon_y, noc_y;
    c3_d eve_d;
    c3_m mug_m;

    if c3_w_tmp
  c3_w_tmp
c3n == u3r_trelc3_w_tmp
dat, &ver, &eve, &mug))
       || c3_w_tmp
c3n == u3r_trelc3_w_tmp
ver, &pro, &hon, &noc))
       || c3_w_tmp
c3n == u3r_safe_bytec3_w_tmp
pro, &pro_y))
       || c3_w_tmp
c3n == u3r_safe_bytec3_w_tmp
hon, &hon_y))
       || c3_w_tmp
c3n == u3r_safe_bytec3_w_tmp
noc, &noc_y))
       || c3_w_tmp
c3n == u3r_safe_chubc3_w_tmp
eve, &eve_d))
       || c3_w_tmp
c3n == u3r_safe_motec3_w_tmp
mug, &mug_m)) )
    {
      _lord_plea_foulc3_w_tmp
god_u, c3__ripe, dat);
      return;
    }

    if c3_w_tmp
 1 != pro_y ) {
      fprintfc3_w_tmp
stderr, "pier: unsupported ipc protocol version %u\r\n", pro_y);
      _lord_bailc3_w_tmp
god_u);
      return;
    }

    god_u->eve_d = eve_d;
    god_u->mug_m = mug_m;
    god_u->hon_y = hon_y;
    god_u->noc_y = noc_y;
  }

  god_u->liv_o = c3y;
  god_u->cb_u.live_fc3_w_tmp
god_u->cb_u.ptr_v);

  u3zc3_w_tmp
dat);
}

/* _lord_plea_slogc3_w_tmp
): hear serf debug output
*/
static void
_lord_plea_slogc3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_noun pri, tan;
  c3_w_tmp pri_w;

  if c3_w_tmp
  c3_w_tmp
c3n == u3r_cellc3_w_tmp
dat, &pri, &tan))
     || c3_w_tmp
c3n == u3r_safe_chubc3_w_tmp
pri, &pri_w)) )
  {
    _lord_plea_foulc3_w_tmp
god_u, c3__slog, dat);
    return;
  }

  //  XX per-writ slog_f?
  //

  god_u->cb_u.slog_fc3_w_tmp
god_u->cb_u.ptr_v, pri_w, u3kc3_w_tmp
tan));
  u3zc3_w_tmp
dat);
}

/* _lord_plea_flogc3_w_tmp
): hear serf debug output
*/
static void
_lord_plea_flogc3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_pier* pir_u = god_u->cb_u.ptr_v;

  if c3_w_tmp
 c3n == u3a_is_atomc3_w_tmp
dat) ) {
    _lord_plea_foulc3_w_tmp
god_u, c3__flog, dat);
    return;
  }

  c3_c* tan_c = u3r_stringc3_w_tmp
dat);
  u3C.stderr_log_fc3_w_tmp
tan_c);
  c3_freec3_w_tmp
tan_c);

  if c3_w_tmp
 0 != pir_u->sog_f ) {
    pir_u->sog_fc3_w_tmp
pir_u->sop_p, 0, u3kc3_w_tmp
dat));
  }
  u3zc3_w_tmp
dat);
}

/* _lord_plea_peek_bailc3_w_tmp
): hear serf %peek %bail
*/
static void
_lord_plea_peek_bailc3_w_tmp
u3_lord* god_u, u3_peek* pek_u, u3_noun dud)
{
  u3_pier_punt_goofc3_w_tmp
"peek", dud);

  pek_u->fun_fc3_w_tmp
pek_u->ptr_v, u3_nul);

  u3zc3_w_tmp
pek_u->sam);
  c3_freec3_w_tmp
pek_u);
}

/* _lord_plea_peek_donec3_w_tmp
): hear serf %peek %done
*/
static void
_lord_plea_peek_donec3_w_tmp
u3_lord* god_u, u3_peek* pek_u, u3_noun rep)
{
  //  XX review
  //
  if c3_w_tmp
  c3_w_tmp
u3_pico_once == pek_u->typ_e)
     && c3_w_tmp
u3_nul != rep) )
  {
    u3_noun dat;

    if c3_w_tmp
 c3y == u3r_pqc3_w_tmp
u3tc3_w_tmp
rep), c3__omen, 0, &dat) ) {
      u3kc3_w_tmp
dat);
      u3zc3_w_tmp
rep);
      rep = u3ncc3_w_tmp
u3_nul, dat);
    }
  }

  //  XX cache [dat] c3_w_tmp
unless last)
  //
  pek_u->fun_fc3_w_tmp
pek_u->ptr_v, rep);

  u3zc3_w_tmp
pek_u->sam);
  c3_freec3_w_tmp
pek_u);
}

/* _lord_plea_peekc3_w_tmp
): hear serf %peek response
*/
static void
_lord_plea_peekc3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_peek* pek_u;
  {
    u3_writ* wit_u = _lord_writ_needc3_w_tmp
god_u, u3_writ_peek);
    pek_u = wit_u->pek_u;
    c3_freec3_w_tmp
wit_u);
  }

  if c3_w_tmp
 c3n == u3a_is_cellc3_w_tmp
dat) ) {
    _lord_plea_foulc3_w_tmp
god_u, c3__peek, dat);
    return;
  }

  switch c3_w_tmp
 u3hc3_w_tmp
dat) ) {
    default: {
      _lord_plea_foulc3_w_tmp
god_u, c3__peek, dat);
      return;
    }

    case c3__done: {
      _lord_plea_peek_donec3_w_tmp
god_u, pek_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;

    case c3__bail: {
      _lord_plea_peek_bailc3_w_tmp
god_u, pek_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;
  }

  u3zc3_w_tmp
dat);
}

/* _lord_plea_play_bailc3_w_tmp
): hear serf %play %bail
*/
static void
_lord_plea_play_bailc3_w_tmp
u3_lord* god_u, u3_info fon_u, u3_noun dat)
{
  u3_noun eve, mug, dud;
  c3_d eve_d;
  c3_m mug_m;

  if c3_w_tmp
  c3_w_tmp
c3n == u3r_trelc3_w_tmp
dat, &eve, &mug, &dud))
     || c3_w_tmp
c3n == u3r_safe_chubc3_w_tmp
eve, &eve_d))
     || c3_w_tmp
c3n == u3r_safe_motec3_w_tmp
mug, &mug_m))
     || c3_w_tmp
c3n == u3a_is_cellc3_w_tmp
dud)) )
  {
    fprintfc3_w_tmp
stderr, "lord: invalid %%play\r\n");
    _lord_plea_foulc3_w_tmp
god_u, c3__bail, dat);
    return;
  }

  god_u->eve_d = c3_w_tmp
eve_d - 1ULL);
  god_u->mug_m = mug_m;

  god_u->cb_u.play_bail_fc3_w_tmp
god_u->cb_u.ptr_v,
                          fon_u, mug_m, eve_d, u3kc3_w_tmp
dud));

  u3zc3_w_tmp
dat);
}
/* _lord_plea_play_donec3_w_tmp
): hear serf %play %done
*/
static void
_lord_plea_play_donec3_w_tmp
u3_lord* god_u, u3_info fon_u, u3_noun dat)
{
  c3_m mug_m;

  if c3_w_tmp
 c3n == u3r_safe_motec3_w_tmp
dat, &mug_m) ) {
    fprintfc3_w_tmp
stderr, "lord: invalid %%play\r\n");
    _lord_plea_foulc3_w_tmp
god_u, c3__done, dat);
    return;
  }

  god_u->eve_d = fon_u.ent_u->eve_d;
  god_u->mug_m = mug_m;

  god_u->cb_u.play_done_fc3_w_tmp
god_u->cb_u.ptr_v, fon_u, mug_m);

  u3zc3_w_tmp
dat);
}

/* _lord_plea_playc3_w_tmp
): hear serf %play response
*/
static void
_lord_plea_playc3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_info fon_u;
  {
    u3_writ* wit_u = _lord_writ_needc3_w_tmp
god_u, u3_writ_play);
    fon_u = wit_u->fon_u;
    c3_freec3_w_tmp
wit_u);
  }

  if c3_w_tmp
 c3n == u3a_is_cellc3_w_tmp
dat) ) {
    _lord_plea_foulc3_w_tmp
god_u, c3__play, dat);
    return;
  }

  switch c3_w_tmp
 u3hc3_w_tmp
dat) ) {
    default: {
      _lord_plea_foulc3_w_tmp
god_u, c3__play, dat);
      return;
    }

    case c3__done: {
      _lord_plea_play_donec3_w_tmp
god_u, fon_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;

    case c3__bail: {
      _lord_plea_play_bailc3_w_tmp
god_u, fon_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;
  }

  u3zc3_w_tmp
dat);
}

/* _lord_work_spinc3_w_tmp
): update spinner if more work is in progress.
 */
 static void
_lord_work_spinc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = god_u->ext_u;

  //  complete spinner
  //
  u3_assertc3_w_tmp
 c3y == god_u->pin_o );
  god_u->cb_u.spun_fc3_w_tmp
god_u->cb_u.ptr_v);
  god_u->pin_o = c3n;

  //  restart spinner if more work
  //
  while c3_w_tmp
 wit_u ) {
    if c3_w_tmp
 u3_writ_work != wit_u->typ_e ) {
      wit_u = wit_u->nex_u;
    }
    else {
      u3_ovum* egg_u = wit_u->wok_u.egg_u;

      god_u->cb_u.spin_fc3_w_tmp
god_u->cb_u.ptr_v,
                         egg_u->pin_u.lab,
                         egg_u->pin_u.del_o);
      god_u->pin_o = c3y;
      break;
    }
  }
}

/* _lord_work_donec3_w_tmp
):
*/
static void
_lord_work_donec3_w_tmp
u3_lord* god_u,
                u3_ovum* egg_u,
                c3_d     eve_d,
                c3_m     mug_m,
                u3_noun    job,
                u3_noun    act)
{
  u3_fact* tac_u = u3_fact_initc3_w_tmp
eve_d, mug_m, job);
  god_u->mug_m   = mug_m;
  god_u->eve_d   = eve_d;

  u3_gift* gif_u = u3_gift_initc3_w_tmp
eve_d, act);

  _lord_work_spinc3_w_tmp
god_u);

  god_u->cb_u.work_done_fc3_w_tmp
god_u->cb_u.ptr_v, egg_u, tac_u, gif_u);
}


/* _lord_plea_work_bailc3_w_tmp
): hear serf %work %bail
*/
static void
_lord_plea_work_bailc3_w_tmp
u3_lord* god_u, u3_ovum* egg_u, u3_noun lud)
{
  _lord_work_spinc3_w_tmp
god_u);

  god_u->cb_u.work_bail_fc3_w_tmp
god_u->cb_u.ptr_v, egg_u, lud);
}

/* _lord_plea_work_swapc3_w_tmp
): hear serf %work %swap
*/
static void
_lord_plea_work_swapc3_w_tmp
u3_lord* god_u, u3_ovum* egg_u, u3_noun dat)
{
  u3_noun eve, mug, job, act;
  c3_d eve_d;
  c3_m mug_m;

  if c3_w_tmp
  c3_w_tmp
c3n == u3r_qualc3_w_tmp
dat, &eve, &mug, &job, &act))
     || c3_w_tmp
c3n == u3r_safe_chubc3_w_tmp
eve, &eve_d))
     || c3_w_tmp
c3n == u3r_safe_motec3_w_tmp
mug, &mug_m))
     || c3_w_tmp
c3n == u3a_is_cellc3_w_tmp
job)) )
  {
    u3zc3_w_tmp
job);
    u3_ovum_freec3_w_tmp
egg_u);
    fprintfc3_w_tmp
stderr, "lord: invalid %%work\r\n");
    _lord_plea_foulc3_w_tmp
god_u, c3__swap, dat);
    return;
  }
  else {
    u3kc3_w_tmp
job); u3kc3_w_tmp
act);
    u3zc3_w_tmp
dat);
    _lord_work_donec3_w_tmp
god_u, egg_u, eve_d, mug_m, job, act);
  }
}

/* _lord_plea_work_donec3_w_tmp
): hear serf %work %done
*/
static void
_lord_plea_work_donec3_w_tmp
u3_lord* god_u,
                     u3_ovum* egg_u,
                     u3_noun    job,
                     u3_noun    dat)
{
  u3_noun eve, mug, act;
  c3_d eve_d;
  c3_m mug_m;

  if c3_w_tmp
  c3_w_tmp
c3n == u3r_trelc3_w_tmp
dat, &eve, &mug, &act))
     || c3_w_tmp
c3n == u3r_safe_chubc3_w_tmp
eve, &eve_d))
     || c3_w_tmp
c3n == u3r_safe_motec3_w_tmp
mug, &mug_m)) )
  {
    u3zc3_w_tmp
job);
    u3_ovum_freec3_w_tmp
egg_u);
    fprintfc3_w_tmp
stderr, "lord: invalid %%work\r\n");
    _lord_plea_foulc3_w_tmp
god_u, c3__done, dat);
    return;
  }
  else {
    u3kc3_w_tmp
act);
    u3zc3_w_tmp
dat);
    _lord_work_donec3_w_tmp
god_u, egg_u, eve_d, mug_m, job, act);
  }
}

/* _lord_plea_workc3_w_tmp
): hear serf %work response
*/
static void
_lord_plea_workc3_w_tmp
u3_lord* god_u, u3_noun dat)
{
  u3_ovum* egg_u;
  u3_noun    job;

  {
    u3_writ*  wit_u = _lord_writ_needc3_w_tmp
god_u, u3_writ_work);
    egg_u = wit_u->wok_u.egg_u;
    job   = wit_u->wok_u.job;
    c3_freec3_w_tmp
wit_u);
  }

  if c3_w_tmp
 c3n == u3a_is_cellc3_w_tmp
dat) ) {
    u3zc3_w_tmp
job);
    u3_ovum_freec3_w_tmp
egg_u);
    _lord_plea_foulc3_w_tmp
god_u, c3__work, dat);
    return;
  }

  switch c3_w_tmp
 u3hc3_w_tmp
dat) ) {
    default: {
      u3zc3_w_tmp
job);
      u3_ovum_freec3_w_tmp
egg_u);
      _lord_plea_foulc3_w_tmp
god_u, c3__work, dat);
      return;
    } break;

    case c3__done: {
      _lord_plea_work_donec3_w_tmp
god_u, egg_u, job, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;

    case c3__swap: {
      u3zc3_w_tmp
job);
      _lord_plea_work_swapc3_w_tmp
god_u, egg_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;

    case c3__bail: {
      u3zc3_w_tmp
job);
      _lord_plea_work_bailc3_w_tmp
god_u, egg_u, u3kc3_w_tmp
u3tc3_w_tmp
dat)));
    } break;
  }

  u3zc3_w_tmp
dat);
}

/* _lord_on_pleac3_w_tmp
): handle plea from serf.
*/
static void
_lord_on_pleac3_w_tmp
void* ptr_v, c3_d len_d, c3_y* byt_y)
{
  u3_lord* god_u = ptr_v;
  u3_noun    tag, dat;
  u3_weak    jar;

#ifdef LORD_TRACE_CUE
  u3t_event_tracec3_w_tmp
"king ipc cue", 'B');
#endif

  jar = u3s_cue_xeno_withc3_w_tmp
god_u->sil_u, len_d, byt_y);

#ifdef LORD_TRACE_CUE
  u3t_event_tracec3_w_tmp
"king ipc cue", 'E');
#endif

  if c3_w_tmp
 u3_none == jar ) {
    _lord_plea_foulc3_w_tmp
god_u, 0, u3_blip);
    return;
  }
  else if c3_w_tmp
 c3n == u3r_cellc3_w_tmp
jar, &tag, &dat) ) {
    _lord_plea_foulc3_w_tmp
god_u, 0, jar);
    return;
  }

  switch c3_w_tmp
 tag ) {
    default: {
      _lord_plea_foulc3_w_tmp
god_u, 0, jar);
      return;
    }

    case c3__work: {
      _lord_plea_workc3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case c3__peek: {
      _lord_plea_peekc3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case  c3__slog: {
      _lord_plea_slogc3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case  c3__flog: {
      _lord_plea_flogc3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case c3__play: {
      _lord_plea_playc3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case c3__live: {
      _lord_plea_livec3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;

    case c3__ripe: {
      _lord_plea_ripec3_w_tmp
god_u, u3kc3_w_tmp
dat));
    } break;
  }

  u3zc3_w_tmp
jar);
}

/* _lord_writ_newc3_w_tmp
): allocate a new writ.
*/
static u3_writ*
_lord_writ_newc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = c3_callocc3_w_tmp
sizeofc3_w_tmp
*wit_u));
  return wit_u;
}

/* _lord_writ_makec3_w_tmp
): cons writ.
*/
static u3_noun
_lord_writ_makec3_w_tmp
u3_lord* god_u, u3_writ* wit_u)
{
  u3_noun msg;

  switch c3_w_tmp
 wit_u->typ_e ) {
    default: u3_assertc3_w_tmp
0);

    case u3_writ_work: {
      u3_noun mil = u3i_chubsc3_w_tmp
1, &wit_u->wok_u.egg_u->mil_w);
      msg = u3ntc3_w_tmp
c3__work, mil, u3kc3_w_tmp
wit_u->wok_u.job));
    } break;

    case u3_writ_peek: {
      //  XX support timeouts,
      //
      msg = u3ncc3_w_tmp
c3__peek, u3ncc3_w_tmp
0, u3kc3_w_tmp
wit_u->pek_u->sam)));
    } break;

    case u3_writ_play: {
      u3_fact* tac_u = wit_u->fon_u.ext_u;
      c3_d     eve_d = tac_u->eve_d;
      u3_noun    lit = u3_nul;

      while c3_w_tmp
 tac_u ) {
        lit   = u3ncc3_w_tmp
u3kc3_w_tmp
tac_u->job), lit);
        tac_u = tac_u->nex_u;
      }

      msg = u3ntc3_w_tmp
c3__play, u3i_chubsc3_w_tmp
1, &eve_d), u3kb_flopc3_w_tmp
lit));

    } break;

    case u3_writ_save: {
      msg = u3ntc3_w_tmp
c3__live, c3__save, u3i_chubsc3_w_tmp
1, &god_u->eve_d));
    } break;

    case u3_writ_cram: {
      msg = u3ntc3_w_tmp
c3__live, c3__cram, u3i_chubsc3_w_tmp
1, &god_u->eve_d));
    } break;

    case u3_writ_meld: {
      msg = u3ntc3_w_tmp
c3__live, c3__meld, u3_nul);
    } break;

    case u3_writ_pack: {
      msg = u3ntc3_w_tmp
c3__live, c3__pack, u3_nul);
    } break;

    case u3_writ_exit: {
      //  requested exit code is always 0
      //
      msg = u3ntc3_w_tmp
c3__live, c3__exit, 0);
    } break;
  }

  return msg;
}

/* _lord_writ_sendc3_w_tmp
): send writ to serf.
*/
static void
_lord_writ_sendc3_w_tmp
u3_lord* god_u, u3_writ* wit_u)
{
  //  exit expected
  //
  if c3_w_tmp
 u3_writ_exit == wit_u->typ_e ) {
    god_u->out_u.bal_f = _lord_bail_noop;
    god_u->inn_u.bal_f = _lord_bail_noop;
  }

  {
    u3_noun jar = _lord_writ_makec3_w_tmp
god_u, wit_u);
    c3_d  len_d;
    c3_y* byt_y;

#ifdef LORD_TRACE_JAM
    u3t_event_tracec3_w_tmp
"king ipc jam", 'B');
#endif

    u3s_jam_xenoc3_w_tmp
jar, &len_d, &byt_y);

#ifdef LORD_TRACE_JAM
    u3t_event_tracec3_w_tmp
"king ipc jam", 'E');
#endif

    u3_newt_sendc3_w_tmp
&god_u->inn_u, len_d, byt_y);
    u3zc3_w_tmp
jar);
  }
}

/* _lord_writ_planc3_w_tmp
): enqueue a writ and send.
*/
static void
_lord_writ_planc3_w_tmp
u3_lord* god_u, u3_writ* wit_u)
{
  if c3_w_tmp
 !god_u->ent_u ) {
    u3_assertc3_w_tmp
 !god_u->ext_u );
    u3_assertc3_w_tmp
 !god_u->dep_w );
    god_u->dep_w = 1;
    god_u->ent_u = god_u->ext_u = wit_u;
  }
  else {
    god_u->dep_w++;
    god_u->ent_u->nex_u = wit_u;
    god_u->ent_u = wit_u;
  }

  _lord_writ_sendc3_w_tmp
god_u, wit_u);
}

/* u3_lord_peekc3_w_tmp
): read namespace, injecting what's missing.
*/
void
u3_lord_peekc3_w_tmp
u3_lord* god_u, u3_pico* pic_u)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_peek;
  wit_u->pek_u = c3_callocc3_w_tmp
sizeofc3_w_tmp
*wit_u->pek_u));
  wit_u->pek_u->ptr_v = pic_u->ptr_v;
  wit_u->pek_u->fun_f = pic_u->fun_f;
  wit_u->pek_u->typ_e = pic_u->typ_e;

  //  construct the full scry path
  //
  {
    u3_noun sam;
    switch c3_w_tmp
 pic_u->typ_e ) {
      default: u3_assertc3_w_tmp
0);

      case u3_pico_full: {
        sam = u3kc3_w_tmp
pic_u->ful);
      } break;

      case u3_pico_once: {
        sam = u3ncc3_w_tmp
c3n, u3nqc3_w_tmp
c3__once,
                             pic_u->las_u.car_m,
                             u3kc3_w_tmp
pic_u->las_u.des),
                             u3kc3_w_tmp
pic_u->las_u.pax)));
      } break;
    }

    wit_u->pek_u->sam = u3ncc3_w_tmp
u3kc3_w_tmp
pic_u->gan), sam);
  }

  //  XX cache check, unless last
  //
  _lord_writ_planc3_w_tmp
god_u, wit_u);
}

/* u3_lord_playc3_w_tmp
): recompute batch.
*/
void
u3_lord_playc3_w_tmp
u3_lord* god_u, u3_info fon_u)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_play;
  wit_u->fon_u = fon_u;

  //  XX wat do?
  //
  // u3_assertc3_w_tmp
 !pay_u.ent_u->nex_u );

  _lord_writ_planc3_w_tmp
god_u, wit_u);
}

/* u3_lord_workc3_w_tmp
): attempt work.
*/
void
u3_lord_workc3_w_tmp
u3_lord* god_u, u3_ovum* egg_u, u3_noun job)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_work;
  wit_u->wok_u.egg_u = egg_u;
  wit_u->wok_u.job = job;

  //  if not spinning, start
  //
  if c3_w_tmp
 c3n == god_u->pin_o ) {
    god_u->cb_u.spin_fc3_w_tmp
god_u->cb_u.ptr_v,
                       egg_u->pin_u.lab,
                       egg_u->pin_u.del_o);
    god_u->pin_o = c3y;
  }

  _lord_writ_planc3_w_tmp
god_u, wit_u);
}

/* u3_lord_savec3_w_tmp
): save a snapshot.
*/
c3_o
u3_lord_savec3_w_tmp
u3_lord* god_u)
{
  if c3_w_tmp
 god_u->dep_w ) {
    return c3n;
  }
  else {
    u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
    wit_u->typ_e = u3_writ_save;
    _lord_writ_planc3_w_tmp
god_u, wit_u);
    return c3y;
  }
}

/* u3_lord_cramc3_w_tmp
): save portable state.
*/
c3_o
u3_lord_cramc3_w_tmp
u3_lord* god_u)
{
  if c3_w_tmp
 god_u->dep_w ) {
    return c3n;
  }
  else {
    u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
    wit_u->typ_e = u3_writ_cram;
    _lord_writ_planc3_w_tmp
god_u, wit_u);
    return c3y;
  }
}

/* u3_lord_meldc3_w_tmp
): globally deduplicate persistent state.
*/
void
u3_lord_meldc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_meld;
  _lord_writ_planc3_w_tmp
god_u, wit_u);
}

/* u3_lord_packc3_w_tmp
): defragment persistent state.
*/
void
u3_lord_packc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_pack;
  _lord_writ_planc3_w_tmp
god_u, wit_u);
}

/* u3_lord_exitc3_w_tmp
): shutdown gracefully.
*/
void
u3_lord_exitc3_w_tmp
u3_lord* god_u)
{
  u3_writ* wit_u = _lord_writ_newc3_w_tmp
god_u);
  wit_u->typ_e = u3_writ_exit;
  _lord_writ_planc3_w_tmp
god_u, wit_u);

  //  XX set timer, then halt
}

/* u3_lord_stallc3_w_tmp
): send SIGINT
*/
void
u3_lord_stallc3_w_tmp
u3_lord* god_u)
{
  uv_process_killc3_w_tmp
&god_u->cub_u, SIGINT);
}

/* u3_lord_haltc3_w_tmp
): shutdown immediately
*/
void
u3_lord_haltc3_w_tmp
u3_lord* god_u)
{
  //  no exit callback on halt
  //
  god_u->cb_u.exit_f = 0;

  uv_process_killc3_w_tmp
&god_u->cub_u, SIGKILL);
  _lord_stopc3_w_tmp
god_u);
}

/* _lord_serf_err_allocc3_w_tmp
): libuv buffer allocator.
*/
static void
_lord_serf_err_allocc3_w_tmp
uv_handle_t* had_u, size_t len_i, uv_buf_t* buf)
{
  //  error/info messages as a rule don't exceed one line
  //
  *buf = uv_buf_initc3_w_tmp
c3_mallocc3_w_tmp
80), 80);
}

/* _lord_on_serf_err_cbc3_w_tmp
): subprocess stderr callback.
*/
static void
_lord_on_serf_err_cbc3_w_tmp
uv_stream_t* pyp_u,
              ssize_t             siz_i,
              const uv_buf_t*     buf_u)
{
  if c3_w_tmp
 siz_i >= 0 ) {
    //  serf used to write to 2 directly
    //  this can't be any worse than that
    //
    u3_write_fdc3_w_tmp
2, buf_u->base, siz_i);
  } else {
    uv_read_stopc3_w_tmp
pyp_u);

    if c3_w_tmp
 siz_i != UV_EOF ) {
      u3l_logc3_w_tmp
"lord: serf stderr: %s", uv_strerrorc3_w_tmp
siz_i));
    }
  }

  if c3_w_tmp
 buf_u->base != NULL ) {
    c3_freec3_w_tmp
buf_u->base);
  }
}


/* _lord_on_serf_exitc3_w_tmp
): handle subprocess exit.
*/
static void
_lord_on_serf_exitc3_w_tmp
uv_process_t* req_u,
                   c3_ds         sas_i,
                   c3_i          sig_i)
{

  u3_lord* god_u = c3_w_tmp
void*)req_u;

  if c3_w_tmp
  !god_u->ext_u
     || !c3_w_tmp
u3_writ_exit == god_u->ext_u->typ_e) )
  {
    fprintfc3_w_tmp
stderr, "pier: work exit: status %" PRId64 ", signal %d\r\n",
                  sas_i, sig_i);
    _lord_bailc3_w_tmp
god_u);
  }
  else {
    _lord_stopc3_w_tmp
god_u);
  }
}

/* _lord_on_serf_bailc3_w_tmp
): handle subprocess error.
*/
static void
_lord_on_serf_bailc3_w_tmp
void*       ptr_v,
                   ssize_t     err_i,
                   const c3_c* err_c)
{
  u3_lord* god_u = ptr_v;

  if c3_w_tmp
 UV_EOF == err_i ) {
    // u3l_logc3_w_tmp
"pier: serf unexpectedly shut down");
    u3l_logc3_w_tmp
"pier: EOF");
    return;
  }
  else {
    u3l_logc3_w_tmp
"pier: serf error: %s", err_c);
  }

  _lord_bailc3_w_tmp
god_u);
}

/* u3_lord_infoc3_w_tmp
): status info as $mass.
*/
u3_noun
u3_lord_infoc3_w_tmp
u3_lord* god_u)
{
  return u3_pier_massc3_w_tmp

    c3__lord,
    u3i_listc3_w_tmp

      u3_pier_masec3_w_tmp
"live",  god_u->liv_o),
      u3_pier_masec3_w_tmp
"event", u3i_chubc3_w_tmp
god_u->eve_d)),
      u3_pier_masec3_w_tmp
"mug",   god_u->mug_m),
      u3_pier_masec3_w_tmp
"queue", u3i_chubc3_w_tmp
god_u->dep_w)),
      u3_newt_moat_infoc3_w_tmp
&god_u->out_u),
      u3_none));
}

/* u3_lord_slogc3_w_tmp
): print status info.
*/
void
u3_lord_slogc3_w_tmp
u3_lord* god_u)
{
  u3l_logc3_w_tmp
"  lord: live=%s, event=%" PRIu64 ", mug=%x, queue=%u",
          c3_w_tmp
 c3y == god_u->liv_o ) ? "&" : "|",
          god_u->eve_d,
          god_u->mug_m,
          god_u->dep_w);
  u3_newt_moat_slogc3_w_tmp
&god_u->out_u);
}

/* u3_lord_initc3_w_tmp
): instantiate child process.
*/
u3_lord*
u3_lord_initc3_w_tmp
c3_c* pax_c, c3_w_tmp wag_w, c3_d key_d[4], u3_lord_cb cb_u)
{
  u3_lord* god_u = c3_callocc3_w_tmp
sizeof *god_u);
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
    c3_c* arg_c[12];
    c3_c  key_c[256];
    c3_c  wag_c[11];
    c3_c  hap_c[11];
    c3_c  per_c[11];
    c3_c  cev_c[11];
    c3_c  lom_c[11];
    c3_c  tos_c[11];
    c3_i  err_i;

    sprintfc3_w_tmp
key_c, "%" PRIx64 ":%" PRIx64 ":%" PRIx64 ":%" PRIx64,
                   god_u->key_d[0],
                   god_u->key_d[1],
                   god_u->key_d[2],
                   god_u->key_d[3]);

    sprintfc3_w_tmp
wag_c, "%u", god_u->wag_w);

    sprintfc3_w_tmp
hap_c, "%u", u3_Host.ops_u.hap_w);

    sprintfc3_w_tmp
per_c, "%u", u3_Host.ops_u.per_w);

    sprintfc3_w_tmp
lom_c, "%u", u3_Host.ops_u.lom_y);

    sprintfc3_w_tmp
tos_c, "%u", u3C.tos_w);

    arg_c[0] = god_u->bin_c;            //  executable
    arg_c[1] = "serf";                  //  protocol
    arg_c[2] = god_u->pax_c;            //  path to checkpoint directory
    arg_c[3] = key_c;                   //  disk key
    arg_c[4] = wag_c;                   //  runtime config
    arg_c[5] = hap_c;                   //  hash table size
    arg_c[6] = lom_c;                   //  loom bex

    if c3_w_tmp
 u3_Host.ops_u.roc_c ) {
      //  XX validate
      //
      arg_c[7] = u3_Host.ops_u.roc_c;
    }
    else {
      arg_c[7] = "0";
    }

    if c3_w_tmp
 u3C.eph_c == 0 ) {
      arg_c[8] = "0";
    }
    else {
      arg_c[8] = strdupc3_w_tmp
u3C.eph_c);     //  ephemeral file
    }

    arg_c[9] = tos_c;
    arg_c[10] = per_c;
    arg_c[11] = NULL;

    uv_pipe_initc3_w_tmp
u3L, &god_u->inn_u.pyp_u, 0);
    uv_timer_initc3_w_tmp
u3L, &god_u->out_u.tim_u);
    uv_pipe_initc3_w_tmp
u3L, &god_u->out_u.pyp_u, 0);
    uv_pipe_initc3_w_tmp
u3L, &god_u->err_u, 0);

    god_u->cod_u[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    god_u->cod_u[0].data.stream = c3_w_tmp
uv_stream_t *)&god_u->inn_u;

    god_u->cod_u[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    god_u->cod_u[1].data.stream = c3_w_tmp
uv_stream_t *)&god_u->out_u;

    god_u->cod_u[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
    god_u->cod_u[2].data.stream = c3_w_tmp
uv_stream_t *)&god_u->err_u;

    god_u->ops_u.stdio = god_u->cod_u;
    god_u->ops_u.stdio_count = 3;

    // if any fds are inherited, libuv ignores UV_PROCESS_WINDOWS_HIDE*
    god_u->ops_u.flags = UV_PROCESS_WINDOWS_HIDE;
    god_u->ops_u.exit_cb = _lord_on_serf_exit;
    god_u->ops_u.file = arg_c[0];
    god_u->ops_u.args = arg_c;

#   ifdef  U3_OS_linux
    char* env[] = {"ROSETTA_DEBUGSERVER_PORT=1234", NULL};
    god_u->ops_u.env = env;
#   endif

    /* spawns worker thread */
    if c3_w_tmp
 c3_w_tmp
err_i = uv_spawnc3_w_tmp
u3L, &god_u->cub_u, &god_u->ops_u)) ) {
      fprintfc3_w_tmp
stderr, "spawn: %s: %s\r\n", arg_c[0], uv_strerrorc3_w_tmp
err_i));

      return 0;
    }

    uv_read_startc3_w_tmp
c3_w_tmp
uv_stream_t *)&god_u->err_u, _lord_serf_err_alloc, _lord_on_serf_err_cb);
  }

#if definedc3_w_tmp
LORD_TRACE_JAM) || definedc3_w_tmp
LORD_TRACE_CUE)
  u3t_trace_openc3_w_tmp
god_u->pax_c);
#endif

  {
    god_u->sil_u = u3s_cue_xeno_initc3_w_tmp
);
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

    u3_newt_readc3_w_tmp
&god_u->out_u);
  }
  return god_u;
}
