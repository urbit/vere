/// @file

#include "db/lmdb.h"
#include "ent/ent.h"
#include "noun.h"
#include "pace.h"
#include "vere.h"
#include "version.h"
#include "curl/curl.h"

#define PIER_WORK_BATCH 10ULL

#undef VERBOSE_PIER

/* _pier_peek_plan(): add a u3_pico to the peek queue
*/
static void
_pier_peek_plan(u3_pier* pir_u, u3_pico* pic_u)
{
  if (!pir_u->pec_u.ent_u) {
    u3_assert( !pir_u->pec_u.ext_u );
    pir_u->pec_u.ent_u = pir_u->pec_u.ext_u = pic_u;
  }
  else {
    pir_u->pec_u.ent_u->nex_u = pic_u;
    pir_u->pec_u.ent_u = pic_u;
  }

  u3_pier_spin(pir_u);
}

/* _pier_peek_next(): pop u3_pico off of peek queue
*/
static u3_pico*
_pier_peek_next(u3_pier* pir_u)
{
  u3_pico* pic_u = pir_u->pec_u.ext_u;

  if (pic_u) {
    pir_u->pec_u.ext_u = pic_u->nex_u;
    if (!pir_u->pec_u.ext_u) {
      pir_u->pec_u.ent_u = 0;
    }

    pic_u->nex_u = 0;
  }

  return pic_u;
}

/* _pier_work_send(): send new events for processing
*/
static void
_pier_work_send(u3_work* wok_u)
{
  u3_auto* car_u = wok_u->car_u;
  u3_pier* pir_u = wok_u->pir_u;
  u3_lord* god_u = pir_u->god_u;
  c3_w     len_w = 0;

  //  calculate work batch size
  {
    //  XX work depth, or full lord send-stack depth?
    //
    if ( PIER_WORK_BATCH > god_u->dep_w ) {
      len_w = PIER_WORK_BATCH - god_u->dep_w;
    }
  }

  //  send batch
  //
  {
    u3_ovum* egg_u;
    u3_pico* pic_u;
    u3_noun ovo;

    while ( len_w && car_u && (egg_u = u3_auto_next(car_u, &ovo)) ) {
      len_w--;
      u3_lord_work(god_u, egg_u, ovo);

      //  queue events depth first
      //
      car_u = egg_u->car_u;

      //  interleave scry requests
      //
      if ( len_w && (pic_u = _pier_peek_next(pir_u)) )
      {
        len_w--;
        u3_lord_peek(god_u, pic_u);
        u3_pico_free(pic_u);
      }
    }

    //  if there's room left in the batch, fill it up with remaining scries
    //
    while ( len_w-- && (pic_u = _pier_peek_next(pir_u)) )
    {
      u3_lord_peek(god_u, pic_u);
      u3_pico_free(pic_u);
    }
  }
}

/* _pier_work(): advance event processing.
*/
static void
_pier_work(u3_work* wok_u)
{
  u3_pier* pir_u = wok_u->pir_u;

  if ( c3n == pir_u->liv_o ) {
    pir_u->liv_o = u3_auto_live(wok_u->car_u);

    //  all i/o drivers are fully initialized
    //
    if ( c3y == pir_u->liv_o ) {
      //  XX this is when "boot" is actually complete
      //  XX even better would be after neighboring with our sponsor
      //
      u3l_log("pier (%" PRIu64 "): live", pir_u->god_u->eve_d);

      //  XX move callbacking to king
      //
      if ( u3_Host.bot_f ) {
        u3_Host.bot_f();
      }
    }
  }

  if ( u3_psat_work == pir_u->sat_e ) {
    _pier_work_send(wok_u);
  }
  else {
    u3_assert( u3_psat_done == pir_u->sat_e );
  }
}

/* _pier_on_lord_work_spin(): start spinner
*/
static void
_pier_on_lord_work_spin(void* ptr_v, u3_atom pin, c3_o del_o)
{
  u3_pier* pir_u = ptr_v;

  u3_assert(  (u3_psat_wyrd == pir_u->sat_e)
           || (u3_psat_work == pir_u->sat_e)
           || (u3_psat_done == pir_u->sat_e) );

  u3_term_start_spinner(pin, del_o);
}

/* _pier_on_lord_work_spun(): stop spinner
*/
static void
_pier_on_lord_work_spun(void* ptr_v)
{
  u3_pier* pir_u = ptr_v;

  u3_assert(  (u3_psat_wyrd == pir_u->sat_e)
           || (u3_psat_work == pir_u->sat_e)
           || (u3_psat_done == pir_u->sat_e) );

  u3_term_stop_spinner();
}

/* _pier_on_lord_work_done(): event completion from worker.
*/
static void
_pier_on_lord_work_done(void*    ptr_v,
                        u3_ovum* egg_u,
                        u3_noun    act)
{
  u3_pier* pir_u = ptr_v;

  u3_assert(  (u3_psat_work == pir_u->sat_e)
           || (u3_psat_done == pir_u->sat_e) );

#ifdef VERBOSE_PIER
  fprintf(stderr, "pier (%" PRIu64 "): work: done\r\n", tac_u->eve_d);
#endif

  u3_auto_done(egg_u);

  //  XX consider async
  //
  u3_auto_kick(pir_u->wok_u->car_u, act);
  u3z(act);

  _pier_work(pir_u->wok_u);
}

/* _pier_on_lord_work_bail(): event failure from worker.
*/
static void
_pier_on_lord_work_bail(void* ptr_v, u3_ovum* egg_u, u3_noun lud)
{
  u3_pier* pir_u = ptr_v;

#ifdef VERBOSE_PIER
  fprintf(stderr, "pier: work: bail\r\n");
#endif

  u3_assert(  (u3_psat_work == pir_u->sat_e)
           || (u3_psat_done == pir_u->sat_e) );

  u3_auto_bail(egg_u, lud);

  //  XX groace
  //
  if ( pir_u->wok_u ) {
    _pier_work(pir_u->wok_u);
  }
}

/* _pier_work_afte_cb(): run on every loop iteration after i/o polling.
*/
static void
_pier_work_afte_cb(uv_check_t* cek_u)
{
  u3_work* wok_u = cek_u->data;
  _pier_work(wok_u);
}

/* _pier_work_idle_cb(): run on next loop iteration.
*/
static void
_pier_work_idle_cb(uv_idle_t* idl_u)
{
  u3_work* wok_u = idl_u->data;
  _pier_work(wok_u);
  uv_idle_stop(idl_u);
}

/* u3_pier_spin(): (re-)activate idle handler
*/
void
u3_pier_spin(u3_pier* pir_u)
{
  //  XX return c3n instead?
  //
  if (  u3_psat_work == pir_u->sat_e
     || u3_psat_done == pir_u->sat_e )
  {
    u3_work* wok_u = pir_u->wok_u;

    if ( !uv_is_active((uv_handle_t*)&wok_u->idl_u) ) {
      uv_idle_start(&wok_u->idl_u, _pier_work_idle_cb);
    }
  }
}

/* u3_pier_peek(): read namespace.
*/
void
u3_pier_peek(u3_pier*   pir_u,
             u3_noun      gan,
             u3_noun      ful,
             void*      ptr_v,
             u3_peek_cb fun_f)
{
  u3_pico* pic_u = u3_pico_init();

  pic_u->ptr_v = ptr_v;
  pic_u->fun_f = fun_f;
  pic_u->gan   = gan;
  //
  pic_u->typ_e = u3_pico_full;
  pic_u->ful   = ful;

  _pier_peek_plan(pir_u, pic_u);
}

/* u3_pier_peek_last(): read namespace, injecting ship and case.
*/
void
u3_pier_peek_last(u3_pier*   pir_u,
                  u3_noun      gan,
                  c3_m       car_m,
                  u3_atom      des,
                  u3_noun      pax,
                  void*      ptr_v,
                  u3_peek_cb fun_f)
{
  u3_pico* pic_u = u3_pico_init();

  pic_u->ptr_v = ptr_v;
  pic_u->fun_f = fun_f;
  pic_u->gan   = gan;
  //
  pic_u->typ_e       = u3_pico_once;
  pic_u->las_u.car_m = car_m;
  pic_u->las_u.des   = des;
  pic_u->las_u.pax   = pax;

  _pier_peek_plan(pir_u, pic_u);
}

/* _pier_stab(): parse path
*/
static u3_noun
_pier_stab(u3_noun pac)
{
  return u3do("stab", pac);
}

/* _pier_on_scry_done(): scry callback.
*/
static void
_pier_on_scry_done(void* ptr_v, u3_noun nun)
{
  u3_pier* pir_u = ptr_v;
  u3_weak    res = u3r_at(7, nun);

  if (u3_none == res) {
    u3l_log("pier: scry failed");
  }
  else {
    u3_weak out;
    c3_c *ext_c, *pac_c;

    u3l_log("pier: scry succeeded");

    if ( u3_Host.ops_u.puk_c ) {
      pac_c = u3_Host.ops_u.puk_c;
    }
    else {
      pac_c = u3_Host.ops_u.pek_c + 1;
    }

    //  try to serialize as requested
    //
    {
      u3_atom puf = u3i_string(u3_Host.ops_u.puf_c);
      if ( c3y == u3r_sing(c3__jam, puf) ) {
        c3_d len_d;
        c3_y* byt_y;
        u3s_jam_xeno(res, &len_d, &byt_y);
        out = u3i_bytes(len_d, byt_y);
        ext_c = "jam";
        free(byt_y);
      }
      else if ( c3y == u3a_is_atom(res) ) {
        out   = u3dc("scot", u3k(puf), u3k(res));
        ext_c = "txt";
      }
      else {
        u3l_log("pier: cannot export cell as %s", u3_Host.ops_u.puf_c);
        out   = u3_none;
      }
      u3z(puf);
    }

    //  if serialization and export path succeeded, write to disk
    //
    if ( u3_none != out ) {
      c3_c fil_c[256];
      snprintf(fil_c, 256, "%s.%s", pac_c, ext_c);

      u3_unix_save(fil_c, out);
      u3l_log("pier: scry result in %s/.urb/put/%s", u3_Host.dir_c, fil_c);
    }
  }

  u3l_log("pier: exit");
  u3_pier_exit(pir_u);

  u3z(nun);
}

static c3_c*
_resolve_czar(c3_d chu_d, c3_c* who_c)
{
  u3_noun czar = u3dc("scot", 'p', chu_d & ((1 << 8) - 1));
  c3_c* czar_c = u3r_string(czar);

  c3_c url[256];
  c3_w  len_w;
  c3_y* hun_y;

  sprintf(url, "https://%s.urbit.org/~/sponsor/%s", czar_c+1, who_c);

  c3_i ret_i = king_curl_bytes(url, &len_w, &hun_y, 1, 1);
  if (!ret_i) {
    c3_free(czar_c);
    czar_c = (c3_c*)hun_y;
  }

  u3z(czar);
  return czar_c;
}

static c3_o
_czar_boot_data(c3_c* czar_c,
                c3_c* who_c,
                c3_w* bone_w,
                c3_w* czar_glx_w,
                c3_w* czar_ryf_w,
                c3_w* czar_lyf_w,
                c3_w* czar_bon_w,
                c3_w* czar_ack_w)
{
  c3_c url[256];
  c3_w  len_w;
  c3_y* hun_y = 0;

  if ( bone_w != NULL ) {
    sprintf(url, "https://%s.urbit.org/~/boot/%s/%d",
            czar_c+1, who_c, *bone_w );
  } else {
    sprintf(url, "https://%s.urbit.org/~/boot/%s", czar_c+1, who_c);
  }

  c3_o ret_o = c3n;
  c3_i ret_i = king_curl_bytes(url, &len_w, &hun_y, 1, 1);
  if ( !ret_i ) {
    u3_noun jamd = u3i_bytes(len_w, hun_y);
    u3_noun cued = u3qe_cue(jamd);

    u3_noun czar_glx, czar_ryf, czar_lyf, czar_bon, czar_ack;

    if ( (c3y == u3r_hext(cued, 0, &czar_glx, &czar_ryf,
                          &czar_lyf, &czar_bon, &czar_ack)) &&
         (c3y == u3r_safe_word(czar_glx, czar_glx_w)) &&
         (c3y == u3r_safe_word(czar_ryf, czar_ryf_w)) &&
         (c3y == u3r_safe_word(czar_lyf, czar_lyf_w)) ) {
      if ( c3y == u3du(czar_bon) ) u3r_safe_word(u3t(czar_bon), czar_bon_w);
      if ( c3y == u3du(czar_ack) ) u3r_safe_word(u3t(czar_ack), czar_ack_w);
      ret_o = c3y;
    }

    u3z(jamd);
    u3z(cued);
    c3_free(hun_y);
  }

  return ret_o;
}

/* _pier_work_init(): begin processing new events
*/
static void
_pier_work_init(u3_pier* pir_u)
{
  u3_work* wok_u;

  u3_assert( u3_psat_wyrd == pir_u->sat_e );

  pir_u->sat_e = u3_psat_work;
  pir_u->wok_u = wok_u = c3_calloc(sizeof(*wok_u));
  wok_u->pir_u = pir_u;

  //  initialize post i/o polling handle
  //
  uv_check_init(u3L, &wok_u->cek_u);
  wok_u->cek_u.data = wok_u;
  uv_check_start(&wok_u->cek_u, _pier_work_afte_cb);

  //  initialize idle i/o polling handle
  //
  //    NB, not started
  //
  uv_idle_init(u3L, &wok_u->idl_u);
  wok_u->idl_u.data = wok_u;

  // //  setup u3_lord work callbacks
  // //
  // u3_lord_work_cb cb_u = {
  //   .ptr_v  = wok_u,
  //   .spin_f = _pier_on_lord_work_spin,
  //   .spun_f = _pier_on_lord_work_spun,
  //   .done_f = _pier_on_lord_work_done,
  //   .bail_f = _pier_on_lord_work_bail
  // };
  // u3_lord_work_init(pir_u->god_u, cb_u);

  //  XX this is messy, revise
  //
  if ( u3_Host.ops_u.pek_c ) {
    u3_noun pex = u3do("stab", u3i_string(u3_Host.ops_u.pek_c));
    u3_noun car;
    u3_noun dek;
    u3_noun pax;
    if ( c3n == u3r_trel(pex, &car, &dek, &pax)
      || c3n == u3a_is_cat(car) )
    {
      u3m_p("pier: invalid scry", pex);
      _pier_on_scry_done(pir_u, u3_nul);
    } else {
      //  run the requested scry, jam to disk, then exit
      //
      u3l_log("pier: scry");
      u3_pier_peek_last(pir_u, u3nc(u3_nul, u3_nul), u3k(car), u3k(dek),
                        u3k(pax), pir_u, _pier_on_scry_done);
    }
    u3z(pex);
  }
  else {
    //  initialize i/o drivers
    //
    wok_u->car_u = u3_auto_init(pir_u);
    u3_auto_talk(wok_u->car_u);
    _pier_work(wok_u);
  }
}

static void _pier_wyrd_init(u3_pier*);

static void
_boot_scry_cb(void* vod_p, u3_noun nun)
{
  u3_pier* pir_u = (u3_pier*)vod_p;

  u3_atom who = u3dc("scot", c3__p, u3i_chubs(2, pir_u->who_d));
  c3_c*   who_c = u3r_string(who);

  u3_noun rem, glx, ryf, bon, cur, nex;
  c3_w    glx_w, ryf_w, bon_w, cur_w, nex_w;

  c3_w czar_glx_w, czar_ryf_w, czar_lyf_w, czar_bon_w, czar_ack_w;
  czar_glx_w = czar_ryf_w = czar_lyf_w = czar_bon_w = czar_ack_w = u3_none;

  if ( (c3y == u3r_qual(nun, 0, 0, 0, &rem)) &&
       (c3y == u3r_hext(rem, &glx, &ryf, 0, &bon, &cur, &nex)) ) {
    /*
     * Boot scry succeeded. Proceed to cross reference networking state against
     * sponsoring galaxy.
     */
    glx_w = u3r_word(0, glx); ryf_w = u3r_word(0, ryf);
    bon_w = u3r_word(0, bon); cur_w = u3r_word(0, cur);
    nex_w = u3r_word(0, nex);

    u3_atom czar = u3dc("scot", c3__p, glx_w);
    c3_c*   czar_c = u3r_string(czar);

    if ( c3n == _czar_boot_data(czar_c, who_c, &bon_w,
                                &czar_glx_w, &czar_ryf_w,
                                &czar_lyf_w, &czar_bon_w,
                                &czar_ack_w) ) {
      u3l_log("boot: peer-state unvailable on czar, cannot protect from double-boot");

      _pier_wyrd_init(pir_u);
    } else {
      if ( czar_ryf_w == ryf_w ) {
        c3_w ack_w = cur_w - 1;
        if ( czar_ack_w == u3_none ) {
          // This codepath should never be hit
          u3l_log("boot: message-sink-state unvailable on czar, cannot protect from double-boot");
          _pier_wyrd_init(pir_u);
        } else if ( ( nex_w - cur_w ) >= ( czar_ack_w - ack_w ) ) {
          _pier_wyrd_init(pir_u);
        } else {
          u3l_log("boot: failed: double-boot detected, refusing to boot %s\r\n"
                  "this is an old version of the ship, resume the latest version or breach\r\n"
                  "see https://docs.urbit.org/user-manual/id/guide-to-resets",
                  who_c);
          u3_king_bail();
        }
      } else {
        // Trying to boot old ship after breach
        u3l_log("boot: failed: double-boot detected, refusing to boot %s\r\n"
                "you are trying to boot an existing ship from a keyfile,"
                "resume the latest version of the ship or breach\r\n"
                "see https://docs.urbit.org/user-manual/id/guide-to-resets",
                who_c);
        u3_king_bail();
      }
    }

    u3z(czar);
    c3_free(czar_c);
  } else if ( c3y == u3r_trel(nun, 0, 0, &rem) && rem == 0 ) {
    /*
     * Data not available for boot scry. Check against sponsoring galaxy.
     * If peer state exists exit(1) unless ship has breached,
     * otherwise continue boot.
     */
    c3_c* czar_c = _resolve_czar(pir_u->who_d[0], who_c);

    if ( c3n == _czar_boot_data(czar_c, who_c, 0,
                                &czar_glx_w, &czar_ryf_w,
                                &czar_lyf_w, &czar_bon_w, 0) ) {
      c3_free(czar_c);
      _pier_wyrd_init(pir_u);
    } else {
      // Peer state found under czar
      c3_free(czar_c);
      u3_weak kf_ryf = pir_u->ryf;
      if ( kf_ryf == u3_none ) {
        u3l_log("boot: keyfile rift unavailable, cannot protect from double-boot");
        _pier_wyrd_init(pir_u);
      } else if ( kf_ryf > czar_ryf_w ) {
        // Ship breached, galaxy has not heard about the breach; continue boot
        _pier_wyrd_init(pir_u);
      } else if ( (     kf_ryf == czar_ryf_w ) &&
                  ( czar_bon_w == u3_none ) ) {
        // Ship has breached, continue boot
        _pier_wyrd_init(pir_u);
      } else {
        u3l_log("boot: failed: double-boot detected, refusing to boot %s\r\n"
                "this ship has already been booted elsewhere, "
                "boot the existing pier or breach\r\n"
                "see https://docs.urbit.org/user-manual/id/guide-to-resets",
                who_c);
        u3_king_bail();
      }
    }
  } else {
    /*
     * Boot scry endpoint doesn't exists. Most likely old arvo.
     * Continue boot and hope for the best.
     */
    u3l_log("boot: %%boot scry endpoint doesn't exist, cannot protect from double-boot");
    _pier_wyrd_init(pir_u);
  }
  u3z(nun); u3z(who);
  c3_free(who_c);
}


/* _pier_double_boot_init(): maybe peek for %ax /boot.
*/
static void
_pier_double_boot_init(u3_pier* pir_u)
{
  pir_u->sat_e = u3_psat_wyrd;
  c3_d pi_d = pir_u->who_d[0];
  c3_d pt_d = pir_u->who_d[1];

  if ( (pi_d < 256 && pt_d == 0) || (c3n == u3_Host.ops_u.net) ) {
    // Skip double boot protection for galaxies and local mode ships
    //
    _pier_wyrd_init(pir_u);
  } else {
    // Double boot protection
    //
    u3_pico* pic_u = u3_pico_init();

    pic_u->ptr_v = pir_u;
    pic_u->fun_f = _boot_scry_cb;
    pic_u->gan   = u3nc(u3_nul, u3_nul);
    //
    pic_u->typ_e       = u3_pico_once;
    pic_u->las_u.car_m = c3__ax;
    pic_u->las_u.des   = u3_nul;
    pic_u->las_u.pax   = u3nc(u3i_string("boot"), u3_nul);

    u3_lord_peek(pir_u->god_u, pic_u);
    u3_pico_free(pic_u);
  }
}

/* _pier_wyrd_good(): %wyrd version negotation succeeded.
*/
static void
_pier_wyrd_good(u3_pier* pir_u, u3_ovum* egg_u)
{
  //  restore event callbacks
  //
  {
    u3_lord* god_u = pir_u->god_u;
    god_u->cb_u.work_done_f = _pier_on_lord_work_done;
    god_u->cb_u.work_bail_f = _pier_on_lord_work_bail;
  }

  //  initialize i/o drivers
  //
  _pier_work_init(pir_u);

  //  free %wyrd driver and ovum
  //
  {
    u3_auto* car_u = egg_u->car_u;
    u3_auto_done(egg_u);
    c3_free(car_u);
  }
}

/* _pier_wyrd_fail(): %wyrd version negotation failed.
*/
static void
_pier_wyrd_fail(u3_pier* pir_u, u3_ovum* egg_u, u3_noun lud)
{
  //  XX version negotiation failed, print upgrade message
  //
  u3l_log("pier: version negotation failed\n");

  //  XX only print trace with -v ?
  //
  if ( u3_nul != lud ) {
    u3_auto_bail_slog(egg_u, u3k(u3t(lud)));
  }

  u3z(lud);

  //  free %wyrd driver and ovum
  //
  {
    u3_auto* car_u = egg_u->car_u;
    u3_auto_done(egg_u);
    c3_free(car_u);
  }

  u3_pier_bail(pir_u);
}

//  XX organizing version constants
//
#define VERE_NAME  "vere"
#define VERE_ZUSE  409
#define VERE_LULL  321

/* _pier_wyrd_aver(): check for %wend effect and version downgrade. RETAIN
*/
static c3_o
_pier_wyrd_aver(u3_noun act)
{
  u3_noun fec, kel, ver;

  //    XX review, %wend re: %wyrd optional?
  //
  while ( u3_nul != act ) {
    u3x_cell(act, &fec, &act);

    if ( c3__wend == u3h(fec) ) {
      kel = u3t(fec);

      //  traverse $wynn, check for downgrades
      //
      while ( u3_nul != kel ) {
        u3x_cell(kel, &ver, &kel);

        //  check for %zuse downgrade
        //
        if (  (c3__zuse == u3h(ver))
           && (VERE_ZUSE != u3t(ver)) )
        {
          return c3n;
        }

        //  XX in the future, send %wend to serf
        //  to also negotiate downgrade of nock/hoon/&c?
        //  (we don't want to have to filter effects)
        //
      }
    }
  }

  return c3y;
}

/* _pier_on_lord_wyrd_done(): callback for successful %wyrd event.
*/
static void
_pier_on_lord_wyrd_done(void*    ptr_v,
                        u3_ovum* egg_u,
                        u3_noun    act)
{
  u3_pier* pir_u = ptr_v;

  u3_assert( u3_psat_wyrd == pir_u->sat_e );

  //  arvo's side of version negotiation succeeded
  //  traverse [gif_y] and validate
  //
  if ( c3n == _pier_wyrd_aver(act) ) {
    //  XX messaging, cli argument to bypass
    //
    u3l_log("pier: version negotiation failed; downgrade");
    _pier_wyrd_fail(pir_u, egg_u, u3_nul);
  }
  else {
    //  finalize %wyrd success
    //
    _pier_wyrd_good(pir_u, egg_u);

    //  XX do something with any %wyrd effects
    //
    u3z(act);
  }
}

/* _pier_on_lord_wyrd_bail(): callback for failed %wyrd event.
*/
static void
_pier_on_lord_wyrd_bail(void* ptr_v, u3_ovum* egg_u, u3_noun lud)
{
  u3_pier* pir_u = ptr_v;

  u3_assert( u3_psat_wyrd == pir_u->sat_e );

  //  XX add cli argument to bypass negotiation failure
  //
#if 1
  //  print %wyrd failure and exit
  //
  //    XX check bail mote, retry on %intr, %meme, &c
  //
  _pier_wyrd_fail(pir_u, egg_u, lud);
#else
  //  XX temporary hack to fake %wyrd success
  //
  {
    _pier_wyrd_good(pir_u, egg_u);
    u3z(lud);
  }
#endif
}

/* _pier_wyrd_card(): construct %wyrd.
*/
static u3_noun
_pier_wyrd_card(u3_pier* pir_u)
{
  u3_lord* god_u = pir_u->god_u;
  u3_noun    sen;

  {
    c3_l  sev_l;
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now   = u3m_time_in_tv(&tim_u);
    sev_l = u3r_mug(now);
    sen   = u3dc("scot", c3__uv, sev_l);

    u3z(now);
  }

  //  XX god_u not necessarily available yet, refactor call sites
  //
  u3_noun ver = u3nq(u3i_string(VERE_NAME),
                     u3i_string(U3_VERE_PACE),
                     u3dc("scot", c3__ta, u3i_string(URBIT_VERSION)),
                     u3_nul);
  u3_noun kel = u3nl(u3nc(c3__zuse, VERE_ZUSE),  //  XX from both king and serf?
                     u3nc(c3__lull, VERE_LULL),  //  XX from both king and serf?
                     u3nc(c3__arvo, 235),        //  XX from both king and serf?
                     u3nc(c3__hoon, 136),        //  god_u->hon_y
                     u3nc(c3__nock, 4),          //  god_u->noc_y
                     u3_none);
  return u3nt(c3__wyrd, u3nc(sen, ver), kel);
}

/* _pier_wyrd_init(): send %wyrd.
*/
static void
_pier_wyrd_init(u3_pier* pir_u)
{
  u3_noun cad = _pier_wyrd_card(pir_u);
  u3_noun wir = u3nc(c3__arvo, u3_nul);

  u3_assert( u3_psat_wyrd == pir_u->sat_e );

  u3l_log("vere: checking version compatibility");

  {
    u3_lord* god_u = pir_u->god_u;
    u3_auto* car_u = c3_calloc(sizeof(*car_u));
    u3_ovum* egg_u = u3_ovum_init(0, u3_blip, wir, cad);
    u3_noun    ovo;

    car_u->pir_u = pir_u;
    car_u->nam_m = c3__wyrd;

    u3_auto_plan(car_u, egg_u);

    //  instead of subscribing with u3_auto_peer(),
    //  we swizzle the [god_u] callbacks for full control
    //
    god_u->cb_u.work_done_f = _pier_on_lord_wyrd_done;
    god_u->cb_u.work_bail_f = _pier_on_lord_wyrd_bail;

    u3_assert( u3_auto_next(car_u, &ovo) == egg_u );

    u3_lord_work(god_u, egg_u, ovo);
  }
}

/* _pier_on_lord_slog(): debug printf from worker.
*/
static void
_pier_on_lord_slog(void* ptr_v, c3_w pri_w, u3_noun tan)
{
  u3_pier* pir_u = ptr_v;

  if ( 0 != pir_u->sog_f ) {
    pir_u->sog_f(pir_u->sop_p, pri_w, u3k(tan));
  }

  u3_pier_tank(0, pri_w, tan);
}

/* _pier_on_lord_save(): worker (non-portable) snapshot complete.
*/
static void
_pier_on_lord_save(void* ptr_v)
{
  u3_pier* pir_u = ptr_v;

#ifdef VERBOSE_PIER
  fprintf(stderr, "pier: (%" PRIu64 "): lord: save\r\n", pir_u->god_u->eve_d);
#endif

  // _pier_next(pir_u);
}

static void
_pier_bail_impl(u3_pier* pir_u);

/* _pier_on_lord_exit(): worker shutdown.
*/
static void
_pier_on_lord_exit(void* ptr_v)
{
  u3_pier* pir_u = ptr_v;

  //  the lord has already gone
  //
  pir_u->god_u = 0;

  if ( u3_psat_done != pir_u->sat_e ) {
    u3l_log("pier: serf shutdown unexpected");
    u3_pier_bail(pir_u);
  }
  else {
    _pier_bail_impl(pir_u);
  }
}

/* _pier_on_lord_bail(): worker error.
*/
static void
_pier_on_lord_bail(void* ptr_v)
{
  u3_pier* pir_u = ptr_v;

  //  the lord has already gone
  //
  pir_u->god_u = 0;

  u3_pier_bail(pir_u);
}

/* _pier_on_lord_live(): worker is ready.
*/
static void
_pier_on_lord_live(void* ptr_v, u3_atom who, c3_o fak_o)
{
  u3_pier* pir_u = ptr_v;

  //  XX validate
  //
  u3r_chubs(0, 2, pir_u->who_d, who);
  pir_u->fak_o = fak_o;

  //  early exit, preparing for upgrade
  //
  //    XX check kelvins?
  //
  if ( c3y == u3_Host.pep_o ) {
    u3_pier_exit(pir_u);
  }
  else {
    _pier_double_boot_init(pir_u);
  }
}

/* u3_pier_mass(): construct a $mass branch with noun/list.
*/
u3_noun
u3_pier_mass(u3_atom cod, u3_noun lit)
{
  return u3nt(cod, c3n, lit);
}

/* u3_pier_mase(): construct a $mass leaf.
*/
u3_noun
u3_pier_mase(c3_c* cod_c, u3_noun dat)
{
  return u3nt(u3i_string(cod_c), c3y, dat);
}

/* u3_pier_info(): pier status info as noun.
*/
u3_noun
u3_pier_info(u3_pier* pir_u)
{
  u3_noun nat;

  switch (pir_u->sat_e) {
    default: {
      nat = u3_pier_mass(u3i_string("state-unknown"), u3_nul);
    } break;

    case u3_psat_init: {
      nat = u3_pier_mass(c3__init, u3_nul);
    } break;

    case u3_psat_work: {
      u3_work*  wok_u = pir_u->wok_u;

      nat = u3_pier_mass(c3__work,
        wok_u->car_u
        ? u3nc(u3_pier_mass(c3__auto, u3_auto_info(wok_u->car_u)), u3_nul)
        : u3_nul);
    } break;

    case u3_psat_done: {
      nat = u3_pier_mass(c3__done, u3_nul);
    } break;
  }

  return u3_pier_mass(
    c3__pier,
    u3i_list(
      nat,
      u3_lord_info(pir_u->god_u),
      u3_none));
}

/* u3_pier_slog(): print status info.
*/
void
u3_pier_slog(u3_pier* pir_u)
{
  switch ( pir_u->sat_e ) {
    default: {
      u3l_log("pier: unknown state: %u", pir_u->sat_e);
    } break;

    case u3_psat_init: {
      u3l_log("pier: init");
    } break;

    case u3_psat_work: {
      u3l_log("pier: work");

      {
        u3_work* wok_u = pir_u->wok_u;

        if ( wok_u->car_u ) {
          u3_auto_slog(wok_u->car_u);
        }
      }
    } break;

    case u3_psat_done: {
      u3l_log("pier: done");
    } break;
  }


  if ( pir_u->god_u ) {
    u3_lord_slog(pir_u->god_u);
  }
}

/* _pier_init(): create a pier, loading existing.
*/
static u3_pier*
_pier_init(c3_w wag_w, c3_c* pax_c, u3_weak ryf)
{
  //  create pier
  //
  u3_pier* pir_u = c3_calloc(sizeof(*pir_u));

  pir_u->pax_c = pax_c;
  pir_u->sat_e = u3_psat_init;
  pir_u->liv_o = c3n;
  pir_u->ryf   = ryf;

  // XX revise?
  //
  pir_u->per_s = u3_Host.ops_u.per_s;
  pir_u->pes_s = u3_Host.ops_u.pes_s;
  pir_u->por_s = u3_Host.ops_u.por_s;

  //  initialize compute
  //
  {
    //  XX load/set secrets
    //
    c3_d tic_d[1];            //  ticket (unstretched)
    c3_d sec_d[1];            //  generator (unstretched)
    c3_d key_d[4];            //  secret (stretched)

    key_d[0] = key_d[1] = key_d[2] = key_d[3] = 0;

    u3_lord_cb cb_u = {
      .ptr_v = pir_u,
      .live_f = _pier_on_lord_live,
      .spin_f = _pier_on_lord_work_spin,
      .spun_f = _pier_on_lord_work_spun,
      .slog_f = _pier_on_lord_slog,
      .work_done_f = _pier_on_lord_work_done,
      .work_bail_f = _pier_on_lord_work_bail,
      .save_f = _pier_on_lord_save,
      .bail_f = _pier_on_lord_bail,
      .exit_f = _pier_on_lord_exit
    };

    if ( !(pir_u->god_u = u3_lord_init(pax_c, wag_w, key_d, cb_u)) )
    {
      c3_free(pir_u);
      return 0;
    }
  }

  return pir_u;
}

/* u3_pier_stay(): restart an existing pier.
*/
u3_pier*
u3_pier_stay(c3_w wag_w, u3_noun pax, u3_weak ryf)
{
  u3_pier* pir_u;

  if ( !(pir_u = _pier_init(wag_w, u3r_string(pax), ryf)) ) {
    fprintf(stderr, "pier: stay: init fail\r\n");
    u3_king_bail();
    return 0;
  }

  u3z(pax);

  return pir_u;
}

/* u3_pier_save(): save a non-portable snapshot
*/
c3_o
u3_pier_save(u3_pier* pir_u)
{
#ifdef VERBOSE_PIER
  fprintf(stderr, "pier: (%" PRIu64 "): save: plan\r\n", pir_u->god_u->eve_d);
#endif

  //  XX revise
  //
  if ( u3_psat_work == pir_u->sat_e ) {
    u3_lord_save(pir_u->god_u);
    return c3y;
  }

  return c3n;
}

/* u3_pier_meld(): globally deduplicate persistent state.
*/
void
u3_pier_meld(u3_pier* pir_u)
{
#ifdef VERBOSE_PIER
  fprintf(stderr, "pier: (%" PRIu64 "): meld: plan\r\n", pir_u->god_u->eve_d);
#endif

  u3_lord_meld(pir_u->god_u);
}

/* u3_pier_pack(): defragment persistent state.
*/
void
u3_pier_pack(u3_pier* pir_u)
{
#ifdef VERBOSE_PIER
  fprintf(stderr, "pier: (%" PRIu64 "): meld: plan\r\n", pir_u->god_u->eve_d);
#endif

  u3_lord_pack(pir_u->god_u);
}

/* _pier_work_close_cb(): dispose u3_work after closing handles.
*/
static void
_pier_work_close_cb(uv_handle_t* idl_u)
{
  u3_work* wok_u = idl_u->data;
  c3_free(wok_u);
}

/* _pier_work_close(): close drivers/handles in the u3_psat_work state.
*/
static void
_pier_work_close(u3_work* wok_u)
{
  u3_auto_exit(wok_u->car_u);

  uv_close((uv_handle_t*)&wok_u->cek_u, _pier_work_close_cb);
  uv_close((uv_handle_t*)&wok_u->idl_u, 0);
  wok_u->cek_u.data = wok_u;
}

/* _pier_bail_impl(): immediately shutdown.
*/
static void
_pier_bail_impl(u3_pier* pir_u)
{
  pir_u->sat_e = u3_psat_done;

  //  halt serf
  //
  if ( pir_u->god_u ) {
    u3_lord_halt(pir_u->god_u);
    pir_u->god_u = 0;
  }

  //  close drivers
  //
  if ( pir_u->wok_u ) {
    _pier_work_close(pir_u->wok_u);
    pir_u->wok_u = 0;
  }

  //  XX unlink properly
  //
  u3K.pir_u = 0;

  c3_free(pir_u->pax_c);
  c3_free(pir_u);

  u3_king_done();
}

/* u3_pier_bail(): fatal error.
*/
void
u3_pier_bail(u3_pier* pir_u)
{
  u3_Host.xit_i = 1;
  _pier_bail_impl(pir_u);
}

/* u3_pier_exit(): graceful shutdown.
*/
void
u3_pier_exit(u3_pier* pir_u)
{
  if ( u3_psat_done == pir_u->sat_e ) {
    return;
  }

  pir_u->sat_e = u3_psat_done;

  if ( pir_u->god_u ) {
    u3_lord_exit(pir_u->god_u);
    pir_u->god_u = 0;
  }
  else {
    _pier_bail_impl(pir_u);
  }
}


/* c3_rand(): fill a 512-bit (16-word) buffer.
*/
void
c3_rand(c3_w* rad_w)
{
  if ( 0 != ent_getentropy(rad_w, 64) ) {
    fprintf(stderr, "c3_rand getentropy: %s\n", strerror(errno));
    //  XX review
    //
    u3_king_bail();
  }
}

/* _pier_dump_tape(): dump a tape, old style.  Don't do this.
*/
static void
_pier_dump_tape(FILE* fil_u, u3_noun tep)
{
  u3_noun tap = tep;

  while ( c3y == u3du(tap) ) {
    c3_c car_c;

    //  XX this utf-8 caution is unwarranted
    //
    //    we already write() utf8 directly to streams in term.c
    //
    // if ( u3h(tap) >= 127 ) {
    //   car_c = '?';
    // } else
    car_c = u3h(tap);

    putc(car_c, fil_u);
    tap = u3t(tap);
  }

  u3z(tep);
}

/* _pier_dump_wall(): dump a wall, old style.  Don't do this.
*/
static void
_pier_dump_wall(FILE* fil_u, u3_noun wol)
{
  u3_noun wal = wol;

  while ( u3_nul != wal ) {
    _pier_dump_tape(fil_u, u3k(u3h(wal)));

    wal = u3t(wal);

    if ( u3_nul != wal ) {
      putc(13, fil_u);
      putc(10, fil_u);
    }
  }

  u3z(wol);
}

/* u3_pier_tank(): dump single tank.
*/
void
u3_pier_tank(c3_l tab_l, c3_w pri_w, u3_noun tac)
{
  u3_noun blu = u3_term_get_blew(0);
  c3_l  col_l = u3h(blu);
  FILE* fil_u = u3_term_io_hija();

  //  XX temporary, for urb.py test runner
  //  XX eval --cue also using this;
  //     would be nice to have official way to dump goof to stderr
  //
  if ( c3y == u3_Host.ops_u.dem ) {
    fil_u = stderr;
  }

  if ( c3n == u3_Host.ops_u.tem ) {
    switch ( pri_w ) {
        case 3: fprintf(fil_u, "\033[31m>>> "); break;
        case 2: fprintf(fil_u, "\033[33m>>  "); break;
        case 1: fprintf(fil_u, "\033[32m>   "); break;
        case 0: fprintf(fil_u, "\033[38;5;244m"); break;
    }
  }
  else {
    switch ( pri_w ) {
        case 3: fprintf(fil_u, ">>> "); break;
        case 2: fprintf(fil_u, ">>  "); break;
        case 1: fprintf(fil_u, ">   "); break;
    }
  }

  c3_t bad_t = 0;
  //  if we have no arvo kernel and can't evaluate nock
  //  only print %leaf tanks
  //
  if ( 0 == u3A->roc ) {
    if ( c3__leaf == u3h(tac) ) {
      _pier_dump_tape(fil_u, u3k(u3t(tac)));
    }
  }
  else {
    u3_noun low = u3dc("(slum soft wash)", u3nc(tab_l, col_l), u3k(tac));
    u3_noun wol;
    if (c3y == u3r_cell(low, NULL, &wol)) {
      u3k(wol); u3z(low);
      _pier_dump_wall(fil_u, wol);
    }
    else {
      // low == u3_nul, no need to lose it
      //
      bad_t = 1;
    }
  }

  if ( c3n == u3_Host.ops_u.tem ) {
    fprintf(fil_u, "\033[0m");
  }

  fflush(fil_u);

  u3_term_io_loja(0, fil_u);
  u3z(blu);
  u3z(tac);
  
  if ( bad_t ) {
    u3l_log("%%slog-bad-tank");
  }
}

/* u3_pier_punt(): dump tank list.
*/
void
u3_pier_punt(c3_l tab_l, u3_noun tac)
{
  u3_noun cat = tac;

  while ( c3y == u3du(cat) ) {
    u3_pier_tank(tab_l, 0, u3k(u3h(cat)));
    cat = u3t(cat);
  }

  u3z(tac);
}

/* u3_pier_punt_goof(): dump a [mote tang] crash report.
*/
void
u3_pier_punt_goof(const c3_c* cap_c, u3_noun dud)
{
  u3_noun bud = dud;
  u3_noun mot, tan;

  u3x_cell(dud, &mot, &tan);

  u3l_log("");
  u3_pier_punt(0, u3qb_flop(tan));

  {
    c3_c* mot_c = u3r_string(mot);
    u3l_log("%s: bail: %%%s", cap_c, mot_c);
    c3_free(mot_c);
  }

  u3z(bud);
}

/* u3_pier_punt_ovum(): print ovum details.
*/
void
u3_pier_punt_ovum(const c3_c* cap_c, u3_noun wir, u3_noun tag)
{
  c3_c* tag_c = u3r_string(tag);
  u3_noun riw = u3do("spat", wir);
  c3_c* wir_c = u3r_string(riw);

  u3l_log("%s: %%%s event on %s failed\n", cap_c, tag_c, wir_c);

  c3_free(tag_c);
  c3_free(wir_c);
  u3z(riw);
}

/* u3_pier_sway(): print trace.
*/
void
u3_pier_sway(c3_l tab_l, u3_noun tax)
{
  u3_noun mok = u3dc("mook", 2, tax);

  u3_pier_punt(tab_l, u3k(u3t(mok)));
  u3z(mok);
}

static c3_w
_pier_mark_pico(u3_pico* pic_u)
{
  c3_w siz_w = 0;

  while ( pic_u ) {

    switch ( pic_u->typ_e ) {
      case u3_pico_once: {
        siz_w += u3a_mark_noun(pic_u->las_u.des);
        siz_w += u3a_mark_noun(pic_u->las_u.pax);
      } break;

      case u3_pico_full: {
        siz_w += u3a_mark_noun(pic_u->ful);
      } break;
    }

    pic_u = pic_u->nex_u;
  }

  return siz_w;
}

/* u3_pier_mark(): mark all Loom allocations in all u3_pier structs.
*/
u3m_quac**
u3_pier_mark(u3_pier* pir_u, c3_w *out_w)
{
  u3m_quac** all_u = c3_malloc(4 * sizeof(*all_u));

  all_u[0] = c3_malloc(sizeof(**all_u));
  all_u[0]->nam_c = strdup("drivers");
  if ( pir_u->wok_u ) {
    all_u[0]->qua_u = u3_auto_mark(pir_u->wok_u->car_u, &(all_u[0]->siz_w));
  }
  else {
    all_u[0]->qua_u = 0;
  }

  all_u[1] = u3_lord_mark(pir_u->god_u);

  all_u[2] = c3_malloc(sizeof(**all_u));
  all_u[2]->nam_c = strdup("peeks");
  all_u[2]->siz_w = 4 * _pier_mark_pico(pir_u->pec_u.ext_u);
  all_u[2]->qua_u = 0;

  all_u[3] = 0;

  *out_w = all_u[0]->siz_w + all_u[1]->siz_w + all_u[2]->siz_w;

  return all_u;
}
