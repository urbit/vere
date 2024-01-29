/// @file
///
#include "vere.h"
#include <sys/time.h>
#include <types.h>

#include "noun.h"
#include "ur.h"
#include "cubic.h"

#define FAST_CONVERGENCE  0

const c3_d cee_d = 13; // (0.4*32)
const c3_d bet_d = 3; // (.2*16)


static c3_d
_get_now_micros()
{
  struct timeval tim_u;
  gettimeofday(&tim_u, NULL);
  return (tim_u.tv_sec * 1000000) + tim_u.tv_usec;
}

static c3_d
_cube(c3_d val_d)
{
  return val_d * val_d * val_d;

}

void u3_cbic_init(u3_gage* gag_u)
{
  gag_u->alg_u = c3_calloc(sizeof(u3_cbic));
  gag_u->alg_c = "CUBIC";
}

void u3_cbic_done(u3_gage* gag_u)
{
  free(gag_u->alg_u);
}


void u3_cbic_reset(u3_gage* gag_u)
{
  u3_cbic* cub_u = gag_u->alg_u;
  cub_u->las_w = 0;
  cub_u->poc_d = 0;
  cub_u->rig_w = 0;
  cub_u->del_w = 0;
  cub_u->win_w = 0;
  cub_u->cac_w = 0;
}


void u3_cbic_tcp(u3_cbic* cub_u)
{
  u3_cbic sat_u = cub_u->sat_u;
  u3_cong con_u = cub_u->con_u;

  sat_u.tcp_w = sat_u.tcp_w + ((c3_w)((3*bet_f)/(2-bet_f)) * (sat_u.cac_w/con_u.wnd_w));
  sat_u.cac_w = 0;
  if ( sat_u.tcp_w > con_u.wnd_w ) {
    sat_u.max_w = con_u.wnd_w/(sat_u.tcp_w-con_u.wnd_w);
    if ( sat_u.cnt_w > sat_u.max_w ) {
      sat_u.cnt_w = sat_u.max_w;
    }
  } 
}

void u3_cbic_update(u3_gage* gag_u)
{
  u3_cbic* cub_u = gag_u->alg_u;
  cub_u->cac_w += 1;
  c3_d tim_d = _get_now_micros();
  
  // if no epoch start set, then begin now
  if ( 0 == cub_u->poc_d ) {
    cub_u->poc_d = tim_d;
    if ( ( gag_u->wnd_w < cub_u->las_w ) && FAST_CONVERGENCE ) {
      //sat_u.las_w = con_u.wnd_w * 
      // set origin point to w last max
    } else {
      cub_u->kay_w = 0;
      cub_u->rig_w = gag_u->wnd_w;
    }
    cub_u->cac_w = 1;
    cub_u->win_w = gag_u->wnd_w;
  } 
  c3_d tee_d = tim_d + cub_u->del_w - cub_u->poc_d;
  c3_d tar_d = cub_u->rig_w + ((cee_d * _cube(tee_d - cub_u->kay_w)) >> 5);
  if ( tar_d > gag_u->wnd_w ) {
    cub_u->cnt_w = gag_u->wnd_w / ( tar_d - gag_u->wnd_w );
  } else { 
    cub_u->cnt_w = gag_u->wnd_w * 100;
  }
  if ( c3y == cub_u->tcp_o ) {
    u3_cbic_tcp(cub_u);
  }
}


void u3_cbic_on_ack(u3_gage* gag_u)
{
  c3_w rtt_w = gag_u->rtt_w; // TODO actually update
  u3_cbic* cub_u = gag_u->alg_u;
  if ( 0 == cub_u->del_w ) {
    cub_u->del_w = c3_min(cub_u->del_w, rtt_w);
  } else {
    cub_u->del_w = rtt_w;
  }

  if ( gag_u->wnd_w <= gag_u->sst_w ) {
    gag_u->wnd_w += 1;
  } else {
    if ( gag_u->wnc_w > gag_u->wnd_w ) {
      gag_u->wnd_w += 1;
      gag_u->wnc_w = 0;
    } else {
      gag_u->wnc_w += 1;
    }
  }
}

void u3_cbic_lost(u3_gage* gag_u)
{

}


