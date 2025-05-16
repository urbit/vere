/// @file

#include "noun.h"

#include "vere.h"
#include "ivory.h"
#include "ur/ur.h"
#include <manage.h>

/*
|%
::  +writ: from king to serf
::
::    next steps:
::    - %peek persistent dates (in arvo or serf)?
::    - |mass should be a query of the serf directly
::    - add duct or vane stack for spinner
::
+$  writ
  $%  $:  %live
          $%  [%cram eve=@]
              [%exit cod=@]
              [%save eve=@]
              [%meld ~]
              [%pack ~]
      ==  ==
      [%peek mil=@ sam=*]  :: gang (each path $%([%once @tas @tas path] [beam @tas beam]))
      [%play eve=@ lit=(list ?((pair @da ovum) *))]
      $:  %quiz
          $%  [%quac ~]
              [%quic ~]
      ==  ==
      [%work mil=@ job=(pair @da ovum)]
  ==
::  +plea: from serf to king
::
+$  plea
  $%  [%live ~]
      [%ripe [pro=%1 hon=@ nok=@] eve=@ mug=@]
      [%slog pri=@ tank]
      [%flog cord]
      $:  %peek
          $%  [%done dat=(unit (cask))]
              [%bail dud=goof]
      ==  ==
      $:  %play
          $%  [%done mug=@]
              [%bail eve=@ mug=@ dud=goof]
      ==  ==
      $:  %quiz
          $%  [%quac p=*]
              [%quic p=*]
      ==  ==
      $:  %work
          $%  [%done eve=@ mug=@ fec=(list ovum)]
              [%swap eve=@ mug=@ job=(pair @da ovum) fec=(list ovum)]
              [%bail lud=(list goof)]
      ==  ==
  ==
--
*/

/*  serf memory-threshold levels
*/
enum {
  _serf_mas_init = 0,  //  initial
  _serf_mas_hit1 = 1,  //  past low threshold
  _serf_mas_hit0 = 2   //  have high threshold
};

/*  serf post-op flags
*/
enum {
  _serf_fag_none = 0,       //  nothing to do
  _serf_fag_hit1 = 1 << 0,  //  hit low threshold
  _serf_fag_hit0 = 1 << 1,  //  hit high threshold
  _serf_fag_mute = 1 << 2,  //  mutated state
  _serf_fag_much = 1 << 3,  //  bytecode hack
  _serf_fag_vega = 1 << 4   //  kernel reset
};

/* _serf_quac: convert a quac to a noun.
*/
u3_noun
_serf_quac(u3m_quac* mas_u)
{
  u3_noun list = u3_nul;
  c3_w i_w = 0;
  if ( mas_u->qua_u != NULL ) {
    while ( mas_u->qua_u[i_w] != NULL ) {
      list = u3nc(_serf_quac(mas_u->qua_u[i_w]), list);
      i_w++;
    }
  }
  list = u3kb_flop(list);

  u3_noun mas = u3nt(u3i_string(mas_u->nam_c), u3i_word(mas_u->siz_w), list);

  c3_free(mas_u->nam_c);
  c3_free(mas_u->qua_u);
  c3_free(mas_u);

  return mas;
}

/* _serf_quacs: convert an array of quacs to a noun list.
*/
u3_noun
_serf_quacs(u3m_quac** all_u)
{
  u3_noun list = u3_nul;
  c3_w i_w = 0;
  while ( all_u[i_w] != NULL ) {
    list = u3nc(_serf_quac(all_u[i_w]), list);
    i_w++;
  }
  c3_free(all_u);
  return u3kb_flop(list);
}

/* _serf_print_quacs: print an array of quacs.
*/
void
_serf_print_quacs(FILE* fil_u, u3m_quac** all_u)
{
  fprintf(fil_u, "\r\n");
  c3_w i_w = 0;
  while ( all_u[i_w] != NULL ) {
    u3a_print_quac(fil_u, 0, all_u[i_w]);
    i_w++;
  }
}

/* _serf_grab(): garbage collect, checking for profiling. RETAIN.
*/
static u3_noun
_serf_grab(u3_noun sac, c3_o pri_o)
{
  if ( u3_nul == sac) {
    if ( u3C.wag_w & (u3o_debug_ram | u3o_check_corrupt) ) {
      u3m_grab(sac, u3_none);
    }
    return u3_nul;
  }
  else {
    FILE* fil_u;

#ifdef U3_MEMORY_LOG
    {
      u3_noun wen = u3dc("scot", c3__da, u3k(u3A->now));
      c3_c* wen_c = u3r_string(wen);

      c3_c nam_c[2048];
      snprintf(nam_c, 2048, "%s/.urb/put/mass", u3C.dir_c);

      struct stat st;
      if ( -1 == stat(nam_c, &st) ) {
        c3_mkdir(nam_c, 0700);
      }

      c3_c man_c[2054];
      snprintf(man_c, 2053, "%s/%s-serf.txt", nam_c, wen_c);

      fil_u = c3_fopen(man_c, "w");
      fprintf(fil_u, "%s\r\n", wen_c);

      c3_free(wen_c);
      u3z(wen);
    }
#else
    {
      fil_u = stderr;
    }
#endif

    u3_assert( u3R == &(u3H->rod_u) );

    u3m_quac* pro_u = u3a_prof(fil_u, sac);

    if ( NULL == pro_u ) {
      fflush(fil_u);
      u3z(sac);
      return u3_nul;
    } else {
      u3m_quac** all_u = c3_malloc(sizeof(*all_u) * 11);
      all_u[0] = pro_u;

      u3m_quac** var_u = u3m_mark();
      all_u[1] = var_u[0];
      all_u[2] = var_u[1];
      all_u[3] = var_u[2];
      all_u[4] = var_u[3];
      c3_free(var_u);

      c3_w tot_w = all_u[0]->siz_w + all_u[1]->siz_w + all_u[2]->siz_w
                     + all_u[3]->siz_w + all_u[4]->siz_w;

      all_u[5] = c3_calloc(sizeof(*all_u[5]));
      all_u[5]->nam_c = strdup("space profile");
      all_u[5]->siz_w = u3a_mark_noun(sac) * 4;

      tot_w += all_u[5]->siz_w;

      all_u[6] = c3_calloc(sizeof(*all_u[6]));
      all_u[6]->nam_c = strdup("total marked");
      all_u[6]->siz_w = tot_w;

      all_u[7] = c3_calloc(sizeof(*all_u[7]));
      all_u[7]->nam_c = strdup("free lists");
      all_u[7]->siz_w = u3a_idle(u3R) * 4;

      all_u[8] = c3_calloc(sizeof(*all_u[8]));
      all_u[8]->nam_c = strdup("sweep");
      all_u[8]->siz_w = u3a_sweep() * 4;
      
      all_u[9] = c3_calloc(sizeof(*all_u[9]));
      all_u[9]->nam_c = strdup("loom");
      all_u[9]->siz_w = u3C.wor_i * 4;

      all_u[10] = NULL;

      if ( c3y == pri_o ) {
        _serf_print_quacs(fil_u, all_u);
      }
      fflush(fil_u);

#ifdef U3_MEMORY_LOG
      {
        fclose(fil_u);
      }
#endif

      u3_noun mas = _serf_quacs( all_u);
      u3z(sac);

      return mas;
    }
  }
}

/* u3_serf_grab(): garbage collect.
*/
u3_noun
u3_serf_grab(c3_o pri_o)
{
  u3_noun sac = u3_nul;
  u3_noun res = u3_nul;

  u3_assert( u3R == &(u3H->rod_u) );

  {
    u3_noun sam, gon;

    {
      u3_noun pax = u3nc(c3__whey, u3_nul);
      u3_noun lyc = u3nc(u3_nul, u3_nul);
      sam = u3nt(lyc, c3n, u3nq(c3__once, u3_blip, u3_blip, pax));
    }

    gon = u3m_soft(0, u3v_peek, sam);

    {
      u3_noun tag, dat, val;
      u3x_cell(gon, &tag, &dat);

      if (  (u3_blip == tag)
         && (u3_nul  != dat)
         && (c3y == u3r_pq(u3t(dat), c3__omen, 0, &val))
         && (c3y == u3r_p(val, c3__mass, &sac)) )
      {
        u3k(sac);
      }
    }

    u3z(gon);
  }

  if ( u3_nul != sac ) {
    res = _serf_grab(sac, pri_o);
  }
  else {
    fprintf(stderr, "sac is empty\r\n");
    u3m_quac** var_u = u3m_mark();

    c3_w tot_w = 0;
    c3_w i_w = 0;
    while ( var_u[i_w] != NULL ) {
      tot_w += var_u[i_w]->siz_w;
      u3a_quac_free(var_u[i_w]);
      i_w++;
    }
    c3_free(var_u);

    u3a_print_memory(stderr, "total marked", tot_w / 4);
    u3a_print_memory(stderr, "free lists", u3a_idle(u3R));
    u3a_print_memory(stderr, "sweep", u3a_sweep());
    fprintf(stderr, "\r\n");
  }

  fflush(stderr);

  return res;
}

/* u3_serf_post(): update serf state post-writ.
*/
void
u3_serf_post(u3_serf* sef_u)
{
  if ( sef_u->fag_w & _serf_fag_hit1 ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("serf: threshold 1: %u", u3h_wyt(u3R->cax.per_p));
    }
    u3h_trim_to(u3R->cax.per_p, u3h_wyt(u3R->cax.per_p) / 2);
    u3m_reclaim();
  }

  if ( sef_u->fag_w & _serf_fag_much ) {
    u3m_reclaim();
  }

  if ( sef_u->fag_w & _serf_fag_vega ) {
    u3h_trim_to(u3R->cax.per_p, u3h_wyt(u3R->cax.per_p) / 2);
    u3m_reclaim();
  }

  //  XX this runs on replay too, |mass s/b elsewhere
  //
  if ( sef_u->fag_w & _serf_fag_mute ) {
    u3z(_serf_grab(sef_u->sac, c3y));
    sef_u->sac   = u3_nul;
  }

  if ( sef_u->fag_w & _serf_fag_hit0 ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("serf: threshold 0: per_p %u", u3h_wyt(u3R->cax.per_p));
    }
    u3h_free(u3R->cax.per_p);
    u3R->cax.per_p = u3h_new_cache(u3C.per_w);
    u3a_print_memory(stderr, "serf: pack: gained", u3m_pack());
    u3l_log("");
  }

  if ( u3C.wag_w & u3o_toss ) {
    u3m_toss();
  }

  sef_u->fag_w = _serf_fag_none;
}

/* _serf_curb(): check for memory threshold
*/
static inline c3_t
_serf_curb(c3_w pre_w, c3_w pos_w, c3_w hes_w)
{
  return (pre_w > hes_w) && (pos_w <= hes_w);
}

/* _serf_sure_feck(): event succeeded, send effects.
*/
static u3_noun
_serf_sure_feck(u3_serf* sef_u, c3_w pre_w, u3_noun vir)
{
  //  intercept |mass, observe |reset
  //
  {
    u3_noun riv = vir;
    c3_w    i_w = 0;

    while ( u3_nul != riv ) {
      u3_noun fec = u3t(u3h(riv));

      //  assumes a max of one %mass effect per event
      //
      if ( c3__mass == u3h(fec) ) {
        //  save a copy of the %mass data
        //
        sef_u->sac = u3k(u3t(fec));
        //  replace the %mass data with ~
        //
        //    For efficient transmission to daemon.
        //
        riv = u3kb_weld(u3qb_scag(i_w, vir),
                        u3nc(u3nt(u3k(u3h(u3h(riv))), c3__mass, u3_nul),
                             u3qb_slag(1 + i_w, vir)));
        u3z(vir);
        vir = riv;
        break;
      }

      //  reclaim memory from persistent caches on |reset
      //
      if ( c3__vega == u3h(fec) ) {
        sef_u->fag_w |= _serf_fag_vega;
      }

      riv = u3t(riv);
      i_w++;
    }
  }

  //  after a successful event, we check for memory pressure.
  //
  //    if we've exceeded either of two thresholds, we reclaim
  //    from our persistent caches, and notify the daemon
  //    (via a "fake" effect) that arvo should trim state
  //    (trusting that the daemon will enqueue an appropriate event).
  //    For future flexibility, the urgency of the notification is represented
  //    by a *decreasing* number: 0 is maximally urgent, 1 less so, &c.
  //
  //    high-priority: 2^25 contiguous words remaining (~128 MB)
  //    low-priority:  2^27 contiguous words remaining (~536 MB)
  //
  //    once a threshold is hit, it's not a candidate to be hit again
  //    until memory usage falls below:
  //
  //    high-priority: 2^26 contiguous words remaining (~256 MB)
  //    low-priority:  2^26 + 2^27 contiguous words remaining (~768 MB)
  //
  //    XX these thresholds should trigger notifications sent to the king
  //    instead of directly triggering these remedial actions.
  //
  {
    u3_noun pri = u3_none;
    c3_w  pos_w = u3a_open(u3R);

    //  if contiguous free space shrunk, check thresholds
    //  (and track state to avoid thrashing)
    //
    if ( pos_w < pre_w ) {
      if (  (_serf_mas_hit0 != sef_u->mas_w)
         && _serf_curb(pre_w, pos_w, 1 << 25) )
      {
        sef_u->mas_w  = _serf_mas_hit0;
        sef_u->fag_w |= _serf_fag_hit0;
        pri           = 0;
      }
      else if (  (_serf_mas_init == sef_u->mas_w)
              && _serf_curb(pre_w, pos_w, 1 << 27) )
      {
        sef_u->mas_w  = _serf_mas_hit1;
        sef_u->fag_w |= _serf_fag_hit1;
        pri         = 1;
      }
    }
    else if ( _serf_mas_init != sef_u->mas_w ) {
      if ( ((1 << 26) + (1 << 27)) < pos_w ) {
        sef_u->mas_w = _serf_mas_init;
      }
      else if (  (_serf_mas_hit0 == sef_u->mas_w)
              && ((1 << 26) < pos_w) )
      {
        sef_u->mas_w = _serf_mas_hit1;
      }
    }

    //  reclaim memory from persistent caches periodically
    //
    //    XX this is a hack to work two things
    //    - bytecode caches grow rapidly and can't be simply capped
    //    - we don't make very effective use of our free lists
    //
    if ( !(sef_u->dun_d % 1024ULL) ) {
      sef_u->fag_w |= _serf_fag_much;
    }

    //  notify daemon of memory pressure via "fake" effect
    //
    if ( u3_none != pri ) {
      u3_noun cad = u3nc(u3nt(u3_blip, c3__arvo, u3_nul),
                         u3nc(c3__trim, pri));
      vir = u3nc(cad, vir);
    }
  }

  return vir;
}

/* _serf_sure_core(): event succeeded, save state.
*/
static void
_serf_sure_core(u3_serf* sef_u, u3_noun cor)
{
  sef_u->dun_d = sef_u->sen_d;

  u3z(u3A->roc);
  u3A->roc     = cor;
  u3A->eve_d   = sef_u->dun_d;
  sef_u->mug_l = u3r_mug(u3A->roc);
  sef_u->fag_w |= _serf_fag_mute;
}

/* _serf_sure(): event succeeded, save state and process effects.
*/
static u3_noun
_serf_sure(u3_serf* sef_u, c3_w pre_w, u3_noun par)
{
  //  vir/(list ovum)  list of effects
  //  cor/arvo         arvo core
  //
  u3_noun vir, cor;
  u3x_cell(par, &vir, &cor);

  _serf_sure_core(sef_u, u3k(cor));
  vir = _serf_sure_feck(sef_u, pre_w, u3k(vir));

  u3z(par);
  return vir;
}

/* _serf_make_crud():
*/
static u3_noun
_serf_make_crud(u3_noun job, u3_noun dud)
{
  u3_noun now, ovo, new;
  u3x_cell(job, &now, &ovo);

  new = u3nt(u3i_vint(u3k(now)),
             u3nt(u3_blip, c3__arvo, u3_nul),
             u3nt(c3__crud, dud, u3k(ovo)));

  u3z(job);
  return new;
}

/* _serf_poke(): RETAIN
*/
static u3_noun
_serf_poke(u3_serf* sef_u, c3_c* cap_c, c3_w mil_w, u3_noun job)
{
  u3_noun now, ovo, wen, gon;
  u3x_cell(job, &now, &ovo);

  wen      = u3A->now;
  u3A->now = u3k(now);

#ifdef U3_EVENT_TIME_DEBUG
  struct timeval b4;
  c3_c*       txt_c;

  gettimeofday(&b4, 0);

  {
    u3_noun tag = u3h(u3t(ovo));
    txt_c = u3r_string(tag);

    if (  (c3__belt != tag)
       && (c3__crud != tag) )
    {
      u3l_log("serf: %s (%" PRIu64 ") %s", cap_c, sef_u->sen_d, txt_c);
    }
  }
#endif

  gon = u3m_soft(mil_w, u3v_poke, u3k(ovo));

#ifdef U3_EVENT_TIME_DEBUG
  {
    struct timeval f2, d0;
    c3_w ms_w;
    c3_w clr_w;

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);

    ms_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    clr_w = ms_w > 1000 ? 1 : ms_w < 100 ? 2 : 3; //  red, green, yellow

    if ( clr_w != 2 ) {
      u3l_log("\x1b[3%dm%%%s (%" PRIu64 ") %4d.%02dms\x1b[0m",
              clr_w, txt_c, sef_u->sen_d, ms_w,
              (int) (d0.tv_usec % 1000) / 10);
    }

    c3_free(txt_c);
  }
#endif

  if ( u3_blip != u3h(gon) ) {
    u3z(u3A->now);
    u3A->now = wen;
  }
  else {
    u3z(wen);
  }

  return gon;
}

/* _serf_work():  apply event, capture effects.
*/
static u3_noun
_serf_work(u3_serf* sef_u, c3_w mil_w, u3_noun job)
{
  u3_noun gon;
  c3_w  pre_w = u3a_open(u3R);

  //  event numbers must be continuous
  //
  u3_assert( sef_u->sen_d == sef_u->dun_d);
  sef_u->sen_d++;

  gon = _serf_poke(sef_u, "work", mil_w, job);  // retain

  //  event accepted
  //
  if ( u3_blip == u3h(gon) ) {
    u3_noun vir = _serf_sure(sef_u, pre_w, u3k(u3t(gon)));

    u3z(gon); u3z(job);
    return u3nc(c3__done, u3nt(u3i_chubs(1, &sef_u->dun_d),
                               sef_u->mug_l,
                               vir));
  }
  //  event rejected -- bad ciphertext
  //
  else if ( c3__evil == u3h(gon) ) {
    sef_u->sen_d = sef_u->dun_d;

    u3z(job);
    return u3nt(c3__bail, gon, u3_nul);
  }
  //  event rejected
  //
  else {
    //  stash $goof from first crash
    //
    u3_noun dud = u3k(gon);

    // XX reclaim on %meme first?
    //

    job = _serf_make_crud(job, dud);
    gon = _serf_poke(sef_u, "crud", mil_w, job);  // retain

    //  error notification accepted
    //
    if ( u3_blip == u3h(gon) ) {
      u3_noun vir = _serf_sure(sef_u, pre_w, u3k(u3t(gon)));

      u3z(gon); u3z(dud);
      return u3nc(c3__swap, u3nq(u3i_chubs(1, &sef_u->dun_d),
                                 sef_u->mug_l,
                                 job,
                                 vir));
    }
    //  error notification rejected
    //
    else {
      sef_u->sen_d = sef_u->dun_d;

      // XX reclaim on %meme ?
      //

      u3z(job);
      return u3nq(c3__bail, gon, dud, u3_nul);
    }
  }
}

/* u3_serf_work(): apply event, producing effects.
*/
u3_noun
u3_serf_work(u3_serf* sef_u, c3_w mil_w, u3_noun job)
{
  c3_t  tac_t = !!( u3C.wag_w & u3o_trace );
  c3_c  lab_c[2056];
  u3_noun pro;

  // XX refactor tracing
  //
  if ( tac_t ) {
    u3_noun wir = u3h(u3t(job));
    u3_noun cad = u3h(u3t(u3t(job)));

    {
      c3_c* cad_c = u3m_pretty(cad);
      c3_c* wir_c = u3m_pretty_path(wir);
      snprintf(lab_c, 2056, "work [%s %s]", wir_c, cad_c);
      c3_free(cad_c);
      c3_free(wir_c);
    }

    u3t_event_trace(lab_c, 'B');
  }

  //  %work must be performed against an extant kernel
  //
  u3_assert( 0 != sef_u->mug_l);

  pro = u3nc(c3__work, _serf_work(sef_u, mil_w, job));

  if ( tac_t ) {
    u3t_event_trace(lab_c, 'E');
  }

  return pro;
}

/* _serf_play_life():
*/
static u3_noun
_serf_play_life(u3_serf* sef_u, u3_noun eve)
{
  u3_noun gon;

  u3_assert( 0ULL == sef_u->sen_d );

  {
    u3_noun len = u3qb_lent(eve);
    u3_assert( c3y == u3r_safe_chub(len, &sef_u->sen_d) );
    u3z(len);
  }

  //  install an ivory pill to support stack traces
  //
  //    XX support -J
  //
//  {
//    c3_d  len_d = u3_Ivory_pill_len;
//    c3_y* byt_y = u3_Ivory_pill;
//    u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
//    u3_weak pil;
//
//    if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
//      u3l_log("lite: unable to cue ivory pill");
//      exit(1);
//    }
//
//    u3s_cue_xeno_done(sil_u);
//
//    if ( c3n == u3v_boot_lite(pil)) {
//      u3l_log("lite: boot failed");
//      exit(1);
//    }
//  }

  gon = u3m_soft(0, u3v_life, eve);

  //  lifecycle sequence succeeded
  //
  if ( u3_blip == u3h(gon) ) {
    //  save product as initial arvo kernel
    //
    _serf_sure_core(sef_u, u3k(u3t(gon)));

    u3z(gon);
    return u3nc(c3__done, sef_u->mug_l);
  }
  //  lifecycle sequence failed
  //
  else {
    //  send failure message and trace
    //
    sef_u->dun_d = sef_u->sen_d = 0;

    return u3nq(c3__bail, 0, 0, gon);
  }
}

/* _serf_play_poke(): RETAIN
*/
static u3_noun
_serf_play_poke(u3_noun job)
{
  u3_noun now, ovo, wen, gon;
  u3x_cell(job, &now, &ovo);

  wen      = u3A->now;
  u3A->now = u3k(now);
  gon      = u3m_soft(0, u3v_poke, u3k(ovo));

  if ( u3_blip != u3h(gon) ) {
    u3z(u3A->now);
    u3A->now = wen;
  }
  else {
    u3z(wen);
  }

  return gon;
}

/* _serf_play_list():
*/
static u3_noun
_serf_play_list(u3_serf* sef_u, u3_noun eve)
{
  c3_w pre_w = u3a_open(u3R);
  u3_noun vev = eve;
  u3_noun job, gon;

  while ( u3_nul != eve ) {
    job = u3h(eve);

    //  bump sent event counter
    //
    sef_u->sen_d++;

    gon = _serf_play_poke(job);

    //  event succeeded, save and continue
    //
    if ( u3_blip == u3h(gon) ) {
      //  vir/(list ovum)  list of effects
      //  cor/arvo         arvo core
      //
      u3_noun vir, cor;
      u3x_trel(gon, 0, &vir, &cor);

      _serf_sure_core(sef_u, u3k(cor));

      //  process effects to set u3_serf_post flags
      //
      u3z(_serf_sure_feck(sef_u, pre_w, u3k(vir)));

      u3z(gon);

      //  skip |mass on replay
      //
      u3z(sef_u->sac);
      sef_u->sac = u3_nul;

      eve = u3t(eve);
    }
    //  event failed, stop and send trace
    //
    else {
      //  reset sent event counter
      //
      sef_u->sen_d = sef_u->dun_d;

      //  XX reclaim on meme ?
      //

      //  send failure notification
      //
      u3z(vev);
      return u3nc(c3__bail, u3nt(u3i_chubs(1, &sef_u->dun_d),
                                 sef_u->mug_l,
                                 gon));
    }
  }

  u3z(vev);
  return u3nc(c3__done, sef_u->mug_l);
}

/* u3_serf_play(): apply event list, producing status.
*/
u3_noun
u3_serf_play(u3_serf* sef_u, c3_d eve_d, u3_noun lit)
{
  u3_assert( eve_d == 1ULL + sef_u->sen_d );

  //  XX better condition for no kernel?
  //
  return u3nc(c3__play, ( 0ULL == sef_u->dun_d )
                        ? _serf_play_life(sef_u, lit)
                        : _serf_play_list(sef_u, lit));
}

/* u3_serf_peek(): dereference namespace.
*/
u3_noun
u3_serf_peek(u3_serf* sef_u, c3_w mil_w, u3_noun sam)
{
  c3_t  tac_t = !!( u3C.wag_w & u3o_trace );
  c3_c  lab_c[2056];

  // XX refactor tracing
  //
  if ( tac_t ) {
    c3_c* foo_c = u3m_pretty(u3t(sam));

    {
      snprintf(lab_c, 2056, "peek %s", foo_c);
      c3_free(foo_c);
    }

    u3t_event_trace(lab_c, 'B');
  }


  u3_noun gon = u3m_soft(mil_w, u3v_peek, sam);
  u3_noun pro;

  if ( tac_t ) {
    u3t_event_trace(lab_c, 'E');
  }



  {
    u3_noun tag, dat;
    u3x_cell(gon, &tag, &dat);

    //  read succeeded, produce result
    //
    if ( u3_blip == tag ) {
      pro = u3nc(c3__done, u3k(dat));
      u3z(gon);
    }
    //  read failed, produce trace
    //
    //    NB, reads should *not* fail deterministically
    //
    else {
      pro = u3nc(c3__bail, gon);
    }
  }

  return u3nc(c3__peek, pro);
}

/* _serf_writ_live_exit(): exit on command.
*/
static void
_serf_writ_live_exit(u3_serf* sef_u, c3_w cod_w)
{
  if ( u3C.wag_w & u3o_debug_cpu ) {
    FILE* fil_u;

    {
      u3_noun wen = u3dc("scot", c3__da, u3k(u3A->now));
      c3_c* wen_c = u3r_string(wen);

      c3_c nam_c[2048];
      snprintf(nam_c, 2048, "%s/.urb/put/profile", u3C.dir_c);

      struct stat st;
      if ( -1 == stat(nam_c, &st) ) {
        c3_mkdir(nam_c, 0700);
      }

      c3_c man_c[2054];
      snprintf(man_c, 2053, "%s/%s.txt", nam_c, wen_c);

      fil_u = c3_fopen(man_c, "w");

      c3_free(wen_c);
      u3z(wen);
    }

    u3t_damp(fil_u);

    {
      fclose(fil_u);
    }
  }

  //  XX move to jets.c
  //
  c3_free(u3D.ray_u);

  sef_u->xit_f();

  exit(cod_w);
}

/* _serf_writ_live_save(): save snapshot.
*/
static void
_serf_writ_live_save(u3_serf* sef_u, c3_d eve_d)
{
  if( eve_d != sef_u->dun_d ) {
    fprintf(stderr, "serf (%" PRIu64 "): save failed: %" PRIu64 "\r\n",
                    sef_u->dun_d,
                    eve_d);
    exit(1);
  }

  u3m_save();
}

/* u3_serf_live(): apply %live command [com], producing *ret on c3y.
*/
c3_o
u3_serf_live(u3_serf* sef_u, u3_noun com, u3_noun* ret)
{
  u3_noun tag, dat;

  //  refcounts around snapshots require special handling
  //
  if ( c3n == u3r_cell(com, &tag, &dat) ) {
    u3z(com);
    return c3n;
  }

  switch ( tag ) {
    default: {
      u3z(com);
      return c3n;
    }

    case c3__exit: {
      c3_y cod_y;

      if ( c3n == u3r_safe_byte(dat, &cod_y) ) {
        u3z(com);
        return c3n;
      }

      u3z(com);
      //  NB, doesn't return
      //
      _serf_writ_live_exit(sef_u, cod_y);
      *ret = u3nc(c3__live, u3_nul);
      return c3y;
    }

    //  NB: the %cram $writ only saves the rock, it doesn't load it
    //
    case c3__cram: {
      c3_d eve_d;

      if ( c3n == u3r_safe_chub(dat, &eve_d) ) {
        u3z(com);
        return c3n;
      }

      u3z(com);

      if( eve_d != sef_u->dun_d ) {
        fprintf(stderr, "serf (%" PRIu64 "): cram failed: %" PRIu64 "\r\n",
                        sef_u->dun_d,
                        eve_d);
        return c3n;
      }

      u3l_log("serf (%" PRIu64 "): saving rock", sef_u->dun_d);

      if ( c3n == u3u_cram(sef_u->dir_c, eve_d) ) {
        fprintf(stderr, "serf (%" PRIu64 "): unable to jam state\r\n", eve_d);
        return c3n;
      }

      if ( u3r_mug(u3A->roc) != sef_u->mug_l ) {
        fprintf(stderr, "serf (%" PRIu64 "): mug mismatch 0x%08x 0x%08x\r\n",
                        eve_d, sef_u->mug_l, u3r_mug(u3A->roc));
        return c3n;
      }

      u3m_save();
      u3_serf_grab(c3y);

      *ret = u3nc(c3__live, u3_nul);
      return c3y;
    }

    case c3__pack: {
      if ( u3_nul != dat ) {
        u3z(com);
        return c3n;
      }
      else {
        u3z(com);
        u3a_print_memory(stderr, "serf: pack: gained", u3m_pack());
        *ret = u3nc(c3__live, u3_nul);
        return c3y;
      }
    }

    case c3__meld: {
      if ( u3_nul != dat ) {
        u3z(com);
        return c3n;
      }
      else {
        u3z(com);
        u3a_print_memory(stderr, "serf: meld: gained", u3_meld_all(stderr));
        *ret = u3nc(c3__live, u3_nul);
        return c3y;
      }
    }

    case c3__save: {
      c3_d eve_d;

      if ( c3n == u3r_safe_chub(dat, &eve_d) ) {
        u3z(com);
        return c3n;
      }

      u3z(com);
      _serf_writ_live_save(sef_u, eve_d);
      *ret = u3nc(c3__live, u3_nul);
      return c3y;
    }
  }
}

/* u3_serf_writ(): apply writ [wit], producing plea [*pel] on c3y.
*/
c3_o
u3_serf_writ(u3_serf* sef_u, u3_noun wit, u3_noun* pel)
{
  u3_noun tag, com;
  c3_o  ret_o;

  if ( c3n == u3r_cell(wit, &tag, &com) ) {
    ret_o = c3n;
  }
  else {
    switch ( tag ) {
      default: {
        ret_o = c3n;
      } break;

      case c3__live: {
        //  since %live can take snapshots, it's refcount protocol is unique
        //
        u3k(com);
        u3z(wit);
        return u3_serf_live(sef_u, com, pel);
      } break;

      case c3__peek: {
        u3_noun tim, sam;
        c3_w  mil_w;

        if ( (c3n == u3r_cell(com, &tim, &sam)) ||
             (c3n == u3r_safe_word(tim, &mil_w)) )
        {
          ret_o = c3n;
        }
        else {
          *pel = u3_serf_peek(sef_u, mil_w, u3k(sam));
          ret_o = c3y;
        }
      } break;

      case c3__play: {
        u3_noun eve, lit;
        c3_d eve_d;

        if ( (c3n == u3r_cell(com, &eve, &lit)) ||
             (c3n == u3a_is_cell(lit)) ||
             (c3n == u3r_safe_chub(eve, &eve_d)) )
        {
          ret_o = c3n;
        }
        else {
          *pel = u3_serf_play(sef_u, eve_d, u3k(lit));
          ret_o = c3y;
        }
      } break;

      case c3__work: {
        u3_noun tim, job;
        c3_w  mil_w;

        if ( (c3n == u3r_cell(com, &tim, &job)) ||
             (c3n == u3r_safe_word(tim, &mil_w)) )
        {
          ret_o = c3n;
        }
        else {
          *pel = u3_serf_work(sef_u, mil_w, u3k(job));
          ret_o = c3y;
        }
      } break;
      case c3__quiz: {
        switch ( u3h(com) ) {
          case c3__quac: {
            u3z(wit);
            u3_noun res = u3_serf_grab(c3n);
            if ( u3_none == res ) {
              ret_o = c3n;
            } else {
              *pel = u3nt(c3__quiz, c3__quac, res);
              ret_o = c3y;
            }
          } break;

          case c3__quic: {
            u3z(wit);
            c3_d pen_d = 4ULL * u3a_open(u3R);
            c3_d dil_d = 4ULL * u3a_idle(u3R);
            fprintf(stderr, "open: %" PRIu64 "\r\n", pen_d);
            fprintf(stderr, "idle: %" PRIu64 "\r\n", dil_d);

            *pel = u3nt(c3__quiz, c3__quic,
                        u3nt(u3nc(c3__open, u3i_chub(pen_d)),
                             u3nc(c3__idle, u3i_chub(dil_d)),
                             u3_nul));
            ret_o = c3y;
          } break;

          default: {
            u3z(wit);
            ret_o = c3n;
          }
        }
      } break;
    }
  }

  if ( tag != c3__quiz ) {
    u3z(wit);
  }
  return ret_o;
}

/* _serf_ripe(): produce initial serf state as [eve=@ mug=@]
*/
static u3_noun
_serf_ripe(u3_serf* sef_u)
{
  // u3l_log("serf: ripe %" PRIu64, sef_u->dun_d);

  sef_u->mug_l = ( 0 == sef_u->dun_d )
                 ? 0
                 : u3r_mug(u3A->roc);

  return u3nc(u3i_chubs(1, &sef_u->dun_d), sef_u->mug_l);
}

/* u3_serf_init(): init or restore, producing status.
*/
u3_noun
u3_serf_init(u3_serf* sef_u)
{
  u3_noun rip;

  {
    c3_w  pro_w = 1;
    c3_y  hon_y = 138;
    c3_y  noc_y = 4;
    u3_noun ver = u3nt(pro_w, hon_y, noc_y);

    rip = u3nt(c3__ripe, ver, _serf_ripe(sef_u));
  }

  //  XX move to u3_serf_post()
  //
  //  measure/print static memory usage if < 1/2 of the loom is available
  //
  // {
  //   c3_w pen_w = u3a_open(u3R);

  //   if ( !(pen_w > (1 << 28)) ) {
  //     fprintf(stderr, "\r\n");
  //     u3a_print_memory(stderr, "serf: contiguous free space", pen_w);
  //     u3_serf_grab();
  //   }
  // }

  sef_u->fag_w = _serf_fag_none;
  sef_u->sac   = u3_nul;

  return rip;
}
