/// @file
///
#include "vere.h"
#include <sys/time.h>
#include <types.h>

#include "noun.h"
#include "ur.h"
#include "cong.h"


typedef struct _u3_cbic {
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
} u3_cbic;


void u3_cbic_init(u3_gage*);

void u3_cbic_done(u3_gage*);

void u3_cbic_on_ack(u3_gage*);

void u3_cbic_lost(u3_gage*);
