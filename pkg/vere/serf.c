/// @file

#include "noun.h"

#include "vere.h"

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
      $:  %work
          $%  [%done eve=@ mug=@ fec=(list ovum)]
              [%swap eve=@ mug=@ job=(pair @da ovum) fec=(list ovum)]
              [%bail lud=(list goof)]
      ==  ==
  ==
--
*/

static u3_noun
_serf_peek_in(u3_cell dat)
{
  u3a_cell* dat_u = u3a_to_ptr(dat);
  return u3v_peek_raw2(dat_u->hed, dat_u->tel);
}

static u3_noun
_serf_peek_soft(u3_serf* sef_u, c3_w mil_w, u3_noun sam)
{
  u3_cell dat = u3nc(u3k(sef_u->roc), sam);
  return u3m_soft(mil_w, _serf_peek_in, dat);
}

/* u3_serf_grab(): garbage collect.
*/
void
u3_serf_grab(u3_serf* sef_u)
{
  FILE*   fil_u = stderr;
  u3_noun   sac = u3_nul;
  u3_serf fes_u = {0};

  if ( !sef_u ) {
    fes_u.roc = u3k(u3A->roc);
    sef_u = &fes_u;
  }

  u3_assert( u3R == &(u3H->rod_u) );

  {
    u3_noun sam, gon;

    {
      u3_noun pax = u3nc(c3__whey, u3_nul);
      u3_noun lyc = u3nc(u3_nul, u3_nul);
      sam = u3nt(lyc, c3n, u3nq(c3__once, u3_blip, u3_blip, pax));
    }

    gon = _serf_peek_soft(sef_u, 0, sam);

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

  fprintf(stderr, "serf: measuring memory:\r\n");

  u3a_mark_noun(sef_u->roc);

  if ( u3_nul == sac ) {
    u3a_print_memory(fil_u, "total marked", u3m_mark(stderr));
    u3a_print_memory(fil_u, "free lists", u3a_idle(u3R));
    u3a_print_memory(fil_u, "sweep", u3a_sweep());
  }
  else {
    c3_w tot_w = 0;

#ifdef U3_MEMORY_LOG
    {
      u3_noun wen = u3dc("scot", c3__da, u3k(u3A->now)); // XX now
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
#endif

    u3_assert( u3R == &(u3H->rod_u) );
    fprintf(fil_u, "\r\n");

    tot_w += u3a_maid(fil_u, "total userspace", u3a_prof(fil_u, 0, sac));
    tot_w += u3m_mark(fil_u);
    tot_w += u3a_maid(fil_u, "space profile", u3a_mark_noun(sac));

    u3a_print_memory(fil_u, "total marked", tot_w);
    u3a_print_memory(fil_u, "free lists", u3a_idle(u3R));
    u3a_print_memory(fil_u, "sweep", u3a_sweep());

#ifdef U3_MEMORY_LOG
    {
      fclose(fil_u);
    }
#endif

  }

  u3z(sac);

  if ( fes_u.roc ) {
    u3z(fes_u.roc);
    fes_u.roc = 0;
  }

  fprintf(fil_u, "\r\n");
  fflush(fil_u);
  u3l_log("");
}

/* u3_serf_post(): update serf state post-writ.
*/
void
u3_serf_post(u3_serf* sef_u)
{
  // if ( sef_u->fag_e & u3_serf_mut_e )
  {
    // u3_assert( &(u3H->rod_u) != u3R );
    sef_u->roc    = u3m_love(sef_u->roc);
    // u3_assert( &(u3H->rod_u) == u3R );
    u3z(u3A->roc);
    u3z(u3A->roc);  // XX really groace
    u3A->roc      = u3k(sef_u->roc);
    u3A->eve_d    = sef_u->dun_d;
    sef_u->fag_e &= ~u3_serf_mut_e;
  }
  // XX wat do?
  // else if u3_serf_inn_e ?

  if ( sef_u->fag_e & u3_serf_rec_e ) {
    u3m_reclaim();
    sef_u->fag_e &= ~u3_serf_rec_e;
  }

  //  XX won't work, requires non-asserting u3a_sweep()
  //
  if ( u3C.wag_w & u3o_check_corrupt ) {
    u3m_grab(sef_u->roc, u3_none);
  }

  if ( sef_u->fag_e & u3_serf_gab_e ) {
    u3_serf_grab(sef_u);
    sef_u->fag_e &= ~u3_serf_gab_e;
  }
  else if ( u3C.wag_w & u3o_debug_ram ) {
    u3m_grab(sef_u->roc, u3_none);
  }

  if ( sef_u->fag_e & u3_serf_pac_e ) {
    u3z(sef_u->roc);
    u3a_print_memory(stderr, "serf: pack: gained", u3m_pack());
    u3l_log("");
    sef_u->roc = u3k(u3A->roc);
    sef_u->fag_e &= ~u3_serf_pac_e;
  }

  if ( sef_u->fag_e & u3_serf_mel_e ) {
    u3z(sef_u->roc);
    u3a_print_memory(stderr, "serf: meld: gained", u3u_meld());
    u3l_log("");
    sef_u->roc = u3k(u3A->roc);
    sef_u->fag_e &= ~u3_serf_mel_e;
  }

  if ( sef_u->fag_e & u3_serf_ram_e ) {
    u3l_log("serf (%" PRIu64 "): saving rock", sef_u->dun_d);

    u3z(sef_u->roc);

    if ( c3n == u3u_cram(sef_u->dir_c, sef_u->dun_d) ) {
      fprintf(stderr, "serf (%" PRIu64 "): unable to jam state\r\n", sef_u->dun_d);
      exit(1);
    }

    sef_u->roc = u3k(u3A->roc);

    if ( u3r_mug(sef_u->roc) != sef_u->mug_l ) {
      fprintf(stderr, "serf (%" PRIu64 "): mug mismatch 0x%08x 0x%08x\r\n",
                      sef_u->dun_d, sef_u->mug_l, u3r_mug(sef_u->roc));
      exit(1);
    }

    u3m_save();
    u3_serf_grab(sef_u);
    sef_u->fag_e &= ~u3_serf_ram_e;
  }


  if ( sef_u->fag_e & u3_serf_sav_e ) {
    u3z(sef_u->roc);
    u3m_save();
    sef_u->roc = u3k(u3A->roc);
    sef_u->fag_e &= ~u3_serf_sav_e;
  }

  if ( sef_u->fag_e & u3_serf_xit_e ) {
    if ( u3C.wag_w & u3o_debug_cpu ) {
      FILE* fil_u;

      {
        u3_noun wen = u3dc("scot", c3__da, u3k(u3A->now)); // XX now
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

    sef_u->fag_e &= ~u3_serf_xit_e;
    sef_u->xit_f();
    exit(sef_u->xit_y);
  }

  if ( u3C.wag_w & u3o_toss ) {
    u3m_toss();
  }
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
        sef_u->fag_e |= u3_serf_gab_e;

        //  replace %mass payload with ~
        //
        //    For efficient transmission to daemon.
        //
        riv = u3kb_weld(u3qb_scag(i_w, vir),
                        u3nc(u3nt(u3k(u3h(u3h(riv))), c3__mass, u3_nul),
                             u3qb_slag(1 + i_w, vir)));

        //  discard original %mass effect, will be retrieved via +peek
        //
        u3z(vir);
        vir = riv;
        break;
      }

      //  reclaim memory from persistent caches on |reset
      //
      if ( c3__vega == u3h(fec) ) {
        sef_u->fag_e |= u3_serf_rec_e;
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
  //    high-priority: 2^22 contiguous words remaining (~16 MB)
  //    low-priority:  2^27 contiguous words remaining (~536 MB)
  //    XX maybe use 2^23 (~32 MB) and 2^26 (~268 MB)?
  //
  //    XX these thresholds should trigger notifications sent to the king
  //    instead of directly triggering these remedial actions.
  //
  {
    u3_noun pri = u3_none;
    c3_w pos_w = u3a_open(u3R);
    c3_w low_w = (1 << 27);
    c3_w hig_w = (1 << 22);

    if ( (pre_w > low_w) && !(pos_w > low_w) ) {
      //  XX set flag(s) in u3V so we don't repeat endlessly?
      //
      sef_u->fag_e |= u3_serf_pac_e;
      sef_u->fag_e |= u3_serf_rec_e;
      pri   = 1;
    }
    else if ( (pre_w > hig_w) && !(pos_w > hig_w) ) {
      sef_u->fag_e |= u3_serf_pac_e;
      sef_u->fag_e |= u3_serf_rec_e;
      pri   = 0;
    }
    //  reclaim memory from persistent caches periodically
    //
    //    XX this is a hack to work two things
    //    - bytecode caches grow rapidly and can't be simply capped
    //    - we don't make very effective use of our free lists
    //
    else if ( 0 == (sef_u->dun_d % 1000ULL) ) {
      sef_u->fag_e |= u3_serf_rec_e;
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
  u3z(sef_u->roc);
  sef_u->dun_d  = sef_u->sen_d;
  sef_u->roc    = cor;
  sef_u->mug_l  = u3r_mug(sef_u->roc);
  sef_u->fag_e |= u3_serf_mut_e;
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

static u3_noun
_serf_poke_in(u3_cell dat)
{
  u3a_cell* dat_u = u3a_to_ptr(dat);
  return u3v_poke_raw2(dat_u->hed, dat_u->tel);
}

static u3_noun
_serf_poke_soft(u3_serf* sef_u, c3_w mil_w, u3_noun sam)
{
  u3_cell dat = u3nc(u3k(sef_u->roc), sam);
  return u3m_soft(mil_w, _serf_poke_in, dat);
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

  gon = _serf_poke_soft(sef_u, mil_w, u3k(job));

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
    gon = _serf_poke_soft(sef_u, mil_w, u3k(job));

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

#ifdef U3_EVENT_TIME_DEBUG
  struct timeval b4;
  c3_c*       txt_c;

  gettimeofday(&b4, 0);

  {
    u3_noun tag = u3h(u3t(u3t(job)));
    txt_c = u3r_string(tag);

    if (  (c3__belt != tag)
       && (c3__crud != tag) )
    {
      u3l_log("serf: (%" PRIu64 ") %s", sef_u->sen_d, txt_c);
    }
  }
#endif

  //  %work must be performed against an extant kernel
  //
  u3_assert( 0 != sef_u->mug_l);

  pro = u3nc(c3__work, _serf_work(sef_u, mil_w, job));

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

  //  ensure zero-initialized kernel
  //
  //    XX wat do?
  //
  // u3A->roc = 0;

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

    gon = _serf_poke_soft(sef_u, 0, u3k(job));

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
      sef_u->fag_e &= ~u3_serf_gab_e;

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
  u3_noun gon = _serf_peek_soft(sef_u, mil_w, sam);
  u3_noun pro;

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
      if ( c3n == u3r_safe_byte(dat, &sef_u->xit_y) ) {
        u3z(com);
        return c3n;
      }

      u3z(com);

      sef_u->fag_e |= u3_serf_xit_e;
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

      sef_u->fag_e |= u3_serf_ram_e;
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
        sef_u->fag_e |= u3_serf_pac_e;
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
        sef_u->fag_e |= u3_serf_mel_e;
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
      if( eve_d != sef_u->dun_d ) {
        fprintf(stderr, "serf (%" PRIu64 "): save failed: %" PRIu64 "\r\n",
                        sef_u->dun_d,
                        eve_d);
        return c3n;
      }

      sef_u->fag_e |= u3_serf_sav_e;
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
    }
  }

  u3z(wit);
  return ret_o;
}

/* u3_serf_init(): init or restore, producing status.
*/
u3_noun
u3_serf_init(u3_serf* sef_u)
{
  u3_noun rip;

  {
    c3_w  pro_w = 1;
    c3_y  hon_y = 139;
    c3_y  noc_y = 4;
    u3_noun ver = u3nt(pro_w, hon_y, noc_y);

    u3_assert( sef_u->dun_d == sef_u->sen_d );
    u3_assert( sef_u->dun_d == u3A->eve_d );

    sef_u->roc   = u3k(u3A->roc);
    sef_u->mug_l = ( 0 == sef_u->dun_d )
                 ? 0
                 : u3r_mug(sef_u->roc);

    rip = u3nt(c3__ripe, ver,
               u3nc(u3i_chubs(1, &sef_u->dun_d), sef_u->mug_l));
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
  //     u3_serf_grab(sef_u);
  //   }
  // }

  sef_u->fag_e = 0;

  return rip;
}
