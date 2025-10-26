#include "windows.h"

#ifndef _RSIGNAL_H
#define _RSIGNAL_H

#define rsignal_setjmp  setjmp
#define rsignal_longjmp longjmp

void rsignal_raise(int sig);
void rsignal_install_handler(int sig, __p_sig_fn_t fn);
void rsignal_deinstall_handler(int sig);
void rsignal_post_longjmp(unsigned long tid, intptr_t* builtin_jb);

#define ITIMER_VIRTUAL 1
struct itimerval {
	struct timeval it_value, it_interval;
};

int rsignal_setitimer(int type, struct itimerval *in, struct itimerval *out);

#endif//_RSIGNAL_H
