/* worker/mars.c
**
**  the main loop of a mars process.
*/
#include "c3.h"
#include "noun.h"
#include "types.h"
#include "vere.h"
#include "db/lmdb.h"
#include <mars.h>
#include <stdio.h>

/* _mars_step_trace(): initialize or rotate trace file.
*/
static void
_mars_step_trace(const c3_c* dir_c)
{
  if ( u3C.wag_w & u3o_trace ) {
    c3_w trace_cnt_w = u3t_trace_cnt();
    if ( trace_cnt_w == 0  && u3t_file_cnt() == 0 ) {
      u3t_trace_open(dir_c);
    }
    else if ( trace_cnt_w >= 100000 ) {
      u3t_trace_close();
      u3t_trace_open(dir_c);
    }
  }
}

/* _mars_poke_play(): replay an event.
*/
static u3_weak
_mars_poke_play(u3_mars* mar_u, const u3_fact* tac_u)
{
  u3_noun gon = u3m_soft(0, u3v_poke_raw, tac_u->job);
  u3_noun tag, dat;
  u3x_cell(gon, &tag, &dat);

  //  event failed, produce trace
  //
  if ( u3_blip != tag ) {
    return gon;
  }

  //  event succeeded, check mug
  //
  {
    u3_noun cor = u3t(dat);
    c3_l  mug_l;

    if ( tac_u->mug_l && (tac_u->mug_l != (mug_l = u3r_mug(cor))) ) {
      fprintf(stderr, "play (%" PRIu64 "): mug mismatch "
                      "expected %08x, actual %08x\r\n",
                      tac_u->eve_d, tac_u->mug_l, mug_l);

      if ( !(u3C.wag_w & u3o_soft_mugs) ) {
        u3z(gon);
        return u3nc(c3__awry, u3_nul);
      }
    }

    u3z(u3A->roc);
    u3A->roc = u3k(cor);
    u3A->eve_d++;
  }

  u3z(gon);
  return u3_none;
}


typedef enum {
  _play_yes_e,  //  success
  _play_mem_e,  //  %meme
  _play_int_e,  //  %intr
  _play_log_e,  //  event log fail
  _play_mug_e,  //  mug mismatch
  _play_bad_e   //  total failure
} _mars_play_e;

/* _mars_play_batch(): replay a batch of events.
*/
static _mars_play_e
_mars_play_batch(u3_mars* mar_u, c3_o mug_o, c3_w bat_w)
{
  u3_disk*      log_u = mar_u->log_u;
  u3_disk_walk* wok_u = u3_disk_walk_init(log_u, mar_u->dun_d + 1, bat_w);
  u3_fact       tac_u;
  u3_noun         dud;

  while ( c3y == u3_disk_walk_live(wok_u) ) {
    if ( c3n == u3_disk_walk_step(wok_u, &tac_u) ) {
      u3_disk_walk_done(wok_u);
      return _play_log_e;
    }

    u3_assert( ++mar_u->sen_d == tac_u.eve_d );

    if ( u3_none != (dud = _mars_poke_play(mar_u, &tac_u)) ) {
      c3_m mot_m;

      mar_u->sen_d = mar_u->dun_d;
      u3_disk_walk_done(wok_u);

      u3_assert( c3y == u3r_safe_word(u3h(dud), &mot_m) );

      switch ( mot_m ) {
        case c3__meme: {
          fprintf(stderr, "play (%" PRIu64 "): %%meme\r\n", tac_u.eve_d);
          u3z(dud);
          return _play_mem_e;
        }

        case c3__intr: {
          fprintf(stderr, "play (%" PRIu64 "): %%intr\r\n", tac_u.eve_d);
          u3z(dud);
          return _play_int_e;
        }

        case c3__awry: {
          fprintf(stderr, "play (%" PRIu64 "): %%awry\r\n", tac_u.eve_d);
          u3z(dud);
          return _play_mug_e;
        }

        default: {
          fprintf(stderr, "play (%" PRIu64 "): failed\r\n", tac_u.eve_d);
          u3_pier_punt_goof("play", dud);
          //  XX say something uplifting
          //
          return _play_bad_e;
        }
      }
    }

    mar_u->dun_d = mar_u->sen_d;
  }

  u3_disk_walk_done(wok_u);

  return _play_yes_e;
}

static c3_o
_mars_do_boot(u3_disk* log_u, c3_d eve_d)
{
  u3_weak eve;
  c3_l  mug_l;

  //  hack to recover structural sharing
  //
  u3m_hate(1 << 18);

  if ( u3_none == (eve = u3_disk_read_list(log_u, 1, eve_d, &mug_l)) ) {
    fprintf(stderr, "boot: read failed\r\n");
    u3m_love(u3_nul);
    return c3n;
  }

  //  hack to recover structural sharing
  //
  eve = u3m_love(u3ke_cue(u3ke_jam(eve)));

  u3l_log("--------------- bootstrap starting ----------------");

  u3l_log("boot: 1-%u", u3qb_lent(eve));

  //  XX check mug if available
  //
  if ( c3n == u3v_boot(eve) ) {
    return c3n;
  }

  u3l_log("--------------- bootstrap complete ----------------");
  return c3y;
}

/* u3_mars_play(): replay up to [eve_d], snapshot every [sap_d].
*/
void
u3_mars_play(u3_mars* mar_u, c3_d eve_d, c3_d sap_d)
{
  u3_disk* log_u = mar_u->log_u;

  if ( !eve_d ) {
    eve_d = log_u->dun_d;
  }
  else if ( eve_d <= mar_u->dun_d ) {
    u3l_log("mars: already computed %" PRIu64 "", eve_d);
    u3l_log("      state=%" PRIu64 ", log=%" PRIu64 "",
            mar_u->dun_d, log_u->dun_d);
    return;
  }
  else {
    eve_d = c3_min(eve_d, log_u->dun_d);
  }

  if ( mar_u->dun_d == log_u->dun_d ) {
    u3l_log("mars: nothing to do!");
    return;
  }

  if ( !mar_u->dun_d ) {
    c3_w lif_w;

    if ( c3n == u3_disk_read_meta(log_u->mdb_u, 0, 0, &lif_w) ) {
      fprintf(stderr, "mars: disk read meta fail\r\n");
      //  XX exit code, cb
      //
      exit(1);
    }

    if ( c3n == _mars_do_boot(mar_u->log_u, lif_w) ) {
      fprintf(stderr, "mars: boot fail\r\n");
      //  XX exit code, cb
      //
      exit(1);
    }

    mar_u->sen_d = mar_u->dun_d = lif_w;
    u3m_save();
  }

  u3l_log("---------------- playback starting ----------------");

  if ( (1ULL + eve_d) == log_u->dun_d ) {
    u3l_log("play: event %" PRIu64 "", log_u->dun_d);
  }
  else if ( eve_d != log_u->dun_d ) {
    u3l_log("play: events %" PRIu64 "-%" PRIu64 " of %" PRIu64 "",
            (c3_d)(1ULL + mar_u->dun_d),
            eve_d,
            log_u->dun_d);
  }
  else {
    u3l_log("play: events %" PRIu64 "-%" PRIu64 "",
            (c3_d)(1ULL + mar_u->dun_d),
            eve_d);
  }

  {
    c3_d pas_d = mar_u->dun_d;  // last snapshot
    c3_d mem_d = 0;             // last event to meme
    c3_w try_w = 0;             // [mem_d] retry count

    while ( mar_u->dun_d < eve_d ) {
      _mars_step_trace(mar_u->dir_c);

      //  XX get batch from args
      //
      switch ( _mars_play_batch(mar_u, c3y, 1024) ) {
        case _play_yes_e: {
          u3m_reclaim();

          if ( sap_d && ((mar_u->dun_d - pas_d) >= sap_d) ) {
            u3m_save();
            pas_d = mar_u->dun_d;
            u3l_log("play (%" PRIu64 "): save", mar_u->dun_d);
          }
          else {
            u3l_log("play (%" PRIu64 "): done", mar_u->dun_d);
          }
        } break;

        case _play_mem_e: {
          if ( mem_d != mar_u->dun_d ) {
            mem_d = mar_u->dun_d;
            try_w = 0;
          }
          else if ( 3 == ++try_w ) {
            fprintf(stderr, "play (%" PRIu64 "): failed, out of loom\r\n",
                            mar_u->dun_d + 1);
            u3m_save();
            //  XX check loom size, suggest --loom X
            //  XX exit code, cb
            //
            u3_disk_exit(log_u);
            exit(1);
          }

          //  XX pack before meld?
          //
          if ( u3C.wag_w & u3o_auto_meld ) {
            u3a_print_memory(stderr, "mars: meld: gained", u3u_meld());
          }
          else {
            u3a_print_memory(stderr, "mars: pack: gained", u3m_pack());
          }
        } break;

        case _play_int_e: {
          fprintf(stderr, "play (%" PRIu64 "): interrupted\r\n", mar_u->dun_d + 1);
          u3m_save();
          //  XX exit code, cb
          //
          u3_disk_exit(log_u);
          exit(1);
        } break;

        //  XX handle any specifically?
        //
        case _play_log_e:
        case _play_mug_e:
        case _play_bad_e: {
          fprintf(stderr, "play (%" PRIu64 "): failed\r\n", mar_u->dun_d + 1);
          u3m_save();
          //  XX exit code, cb
          //
          u3_disk_exit(log_u);
          exit(1);
        } break;
      }
    }
  }

  u3l_log("---------------- playback complete ----------------");
  u3m_save();
}
