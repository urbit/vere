#include "defs.h"

c3_s
c3_sift_short(c3_y buf_y[2]);
c3_w_new
c3_sift_word_new(c3_y buf_y[4]);
c3_d
c3_sift_chub(c3_y byt_y[8]);

void
c3_etch_short(c3_y buf_y[2], c3_s sot_s);
void
c3_etch_word_new(c3_y buf_y[4], c3_w_new wod_w);
void
c3_etch_chub(c3_y byt_y[8], c3_d num_d);

c3_w_new c3_align_w_new(c3_w_new x, c3_w_new al, align_dir hilo);
c3_d c3_align_d(c3_d x, c3_d al, align_dir hilo);
void *c3_align_p(void const * p, size_t al, align_dir hilo);
