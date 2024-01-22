/// @file
///
#include "vere.h"
#include <math.h>

#include "noun.h"
#include "ur.h"

#define FAST_CONVERGENCE  0

const float cee_f = 0.4;
const float bet_f = 0.2;

typedef struct _u3_cbic_stat {
  c3_w     las_w;  // w_last_max
  c3_d     poc_d;  // epoch start
  c3_w     rig_w;   // origin point
  c3_w     del_w;  // delay min
  c3_w     win_w;  // w_tcp
  c3_w     cac_w;  // ack count
  c3_w     kay_w;  // K
  c3_w     cnt_w;  // cnt
  c3_o     tcp_o;  // tcp friendliness
  c3_w     tcp_w;  // w_tcp
  c3_w     max_w;  // max count
} u3_cbic_stat;

typedef struct _u3_cong {
  c3_w     rtt_w;  // rtt
  c3_w     rto_w;  // rto
  c3_w     rtv_w;  // rttvar
  c3_w     wnd_w;  // cwnd
  c3_w     wnc_w; // cwnd_cnt
  c3_w     sst_w;  // ssthresh
  c3_w     con_w;  // counter
} u3_cong;

typedef struct _u3_cbic {
  u3_cong       con_u;
  u3_cbic_stat  sat_u;
} u3_cbic;


void u3_cbic_reset(u3_cbic_stat* sat_u)
{
  sat_u->las_w = 0;
  sat_u->poc_d = 0;
  sat_u->rig_w = 0;
  sat_u->del_w = 0;
  sat_u->win_w = 0;
  sat_u->cac_w = 0;
}

void u3_cbic_init(u3_cbic_stat* sat_u)
{

}

void u3_cbic_tcp(u3_cbic* cub_u)
{
  u3_cbic_stat sat_u = cub_u->sat_u;
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

void u3_cbic_update(u3_cbic* cub_u)
{
  cub_u->sat_u.cac_w += 1;
  u3_cong con_u = cub_u->con_u;
  u3_cbic_stat sat_u = cub_u->sat_u;
  c3_d tim_d = time(NULL);
  
  // if no epoch start set, then begin now
  if ( 0 == sat_u.poc_d ) {
    sat_u.poc_d = tim_d;
    if ( ( con_u.wnd_w < sat_u.las_w ) && FAST_CONVERGENCE ) {
      //sat_u.las_w = con_u.wnd_w * 
      // set origin point to w last max
    } else {
      sat_u.kay_w = 0;
      sat_u.rig_w = con_u.wnd_w;
    }
    sat_u.cac_w = 1;
    sat_u.win_w = con_u.wnd_w;
  } 
  c3_d tee_d = tim_d + sat_u.del_w - sat_u.poc_d;
  c3_d tar_d = sat_u.rig_w + (c3_d)(cee_f * pow((float)tee_d - (float)sat_u.kay_w, 3));
  if ( tar_d > con_u.wnd_w ) {
    sat_u.cnt_w = con_u.wnd_w / ( tar_d - con_u.wnd_w );
  } else { 
    sat_u.cnt_w = con_u.wnd_w * 100;
  }
  if ( c3y == sat_u.tcp_o ) {
    u3_cbic_tcp(cub_u);
  }
}



void u3_cbic_on_ack(u3_cbic* cub_u)
{
  c3_w rtt_w = cub_u->con_u.rtt_w; // TODO actually update
  if ( 0 == cub_u->sat_u.del_w ) {
    cub_u->sat_u.del_w = c3_min(cub_u->sat_u.del_w, rtt_w);
  } else {
    cub_u->sat_u.del_w = rtt_w;
  }

  if ( cub_u->con_u.wnd_w <= cub_u->con_u.sst_w ) {
    cub_u->con_u.wnd_w += 1;
  } else {
    if ( cub_u->con_u.wnc_w > cub_u->con_u.wnd_w ) {
      cub_u->con_u.wnd_w += 1;
      cub_u->con_u.wnc_w = 0;
    } else {
      cub_u->con_u.wnc_w += 1;
    }
  }
}


