/// @file
///
#include "vere.h"
#include <sys/time.h>
#include <types.h>

#include "noun.h"
#include "ur.h"

typedef struct _u3_gage {
  c3_w     rtt_w;  // rtt
  c3_w     rto_w;  // rto
  c3_w     rtv_w;  // rttvar
  c3_w     wnd_w;  // cwnd
  c3_w     wnc_w; // cwnd_cnt
  c3_w     sst_w;  // ssthresh
  c3_w     con_w;  // counter
  void*    alg_u;  // algorithm backpointer
  c3_c*    alg_c;  // algorithm name
} u3_gage;

typedef void (*u3_gage_func)(u3_gage*);


typedef struct _u3_gage_alg {
  u3_gage_func int_f; //initialise
  u3_gage_func don_f; // dispose
  u3_gage_func ack_f; // received ack
  u3_gage_func los_f; // lost
} u3_gage_alg;


