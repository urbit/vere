/// @file
///
#include "vere.h"

#include "noun.h"
#include "ur.h"



typedef struct _u3_cbic_stat {
  c3_w     las_w;  // w_last_max
  c3_d     poc_d;  // epoch start
  c3_w     rig_w   // origin point
  c3_w     del_w;  // delay min
  c3_w     win_w;  // w_tcp
  c3_w     cac_w;  // ack count

} u3_cbic_stat;

typedef struct _u3_cong {
  c3_w     rtt_w;  // rtt
  c3_w     rto_w;  // rto
  c3_w     rtv-w;  // rttvar
  c3_w     wnd_w;  // cwnd
  c3_w     wnc_w; // cwnd_cnt
  c3_w     sst_w;  // ssthresh
  c3_w     con_w;  // counter
} u3_cong;

typedef struct _u3_cbic {
  u3_cong       con_u;
  u3_cbic_stat  sat_u;
} u3_cbic;


void u3_cbic_reset(u3_cbic* sat_u)
{
  sat_u->las_w = 0;
  sat_u->poc_d = 0;
  sat_u->rig_w = 0;
  sat_u->del_w = 0;
  sat_u->win_w = 0;
  sat_u->cac_w = 0;

}

void u3_cbic_init(u3_cbic_stats* sat_u)
{

}

void u3_cbic_update(u3_cbic* cub_u)
{
  c3_w ack_w += 1;
  c3_d tim_d = time();
  if ( 0 == cub_u->sat_w.poc_d ) {
    cub_u->sat_w.poc_w = tim_d;
    if ( cub_u->con_u.wnd_w 
  } else {
    ifk

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
    cub_u->sat_u.wnd_w += 1;
  } else {
    if ( cub_u->con_u.wnc_w > cub_u->con_u.wnd_w ) {
      cub_u->con_u.wnd_w += 1;
      cub_u->con_u.wnc_w = 0;
    } else {
      cub_u->con_u.wnc_w += 1;
    }
  }
}


