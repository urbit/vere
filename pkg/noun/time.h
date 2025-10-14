#ifndef U3_VERE_TIME_H
#define U3_VERE_TIME_H

#include "noun.h"
// #include "vere.h"

/* u3_time_sec_in(): urbit seconds from unix time.
**
** Adjust for future leap secs!
*/
c3_d
u3_time_sec_in(c3_w unx_w);

/* u3_time_sec_out(): unix time from urbit seconds.
**
** Adjust for future leap secs!
*/
c3_w
u3_time_sec_out(c3_d urs_d);

/* u3_time_fsc_in(): urbit fracto-seconds from unix microseconds.
*/
c3_d
u3_time_fsc_in(c3_w usc_w);

/* u3_time_fsc_out: unix microseconds from urbit fracto-seconds.
*/
c3_w
u3_time_fsc_out(c3_d ufc_d);

/* u3_time_msc_out: unix microseconds from urbit fracto-seconds.
*/
c3_w
u3_time_msc_out(c3_d ufc_d);

/* u3_time_in_tv(): urbit time from struct timeval.
*/
u3_atom
u3_time_in_tv(struct timeval* tim_tv);

/* u3_time_out_tv(): struct timeval from urbit time.
*/
void
u3_time_out_tv(struct timeval* tim_tv, u3_noun now);

/* u3_time_in_ts(): urbit time from struct timespec.
*/
u3_atom
u3_time_in_ts(struct timespec* tim_ts);

#if defined(U3_OS_linux) || defined(U3_OS_windows)
/* u3_time_t_in_ts(): urbit time from time_t.
*/
u3_atom
u3_time_t_in_ts(time_t tim);
#endif /* defined(U3_OS_linux) */

/* u3_time_out_ts(): struct timespec from urbit time.
*/
void
u3_time_out_ts(struct timespec* tim_ts, u3_noun now);

/* u3_time_gap_ms(): (wen - now) in ms.
*/
c3_d
u3_time_gap_ms(u3_noun now, u3_noun wen);

/* u3_time_gap_double(): (wen - now) in libev resolution.
*/
double
u3_time_gap_double(u3_noun now, u3_noun wen);

#endif /* ifndef U3_VERE_TIME_H */
