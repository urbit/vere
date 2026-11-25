#include "windows.h"

#ifndef _RSIGNAL_H
#define _RSIGNAL_H

typedef struct {
  jmp_buf jb;
  unsigned long tid;
} rsignal_jmpbuf;

#define rsignal_setjmp(buf)        (buf.tid = GetCurrentThreadId(), setjmp(buf.jb))
#define rsignal_longjmp(buf, val)  if (buf.tid != GetCurrentThreadId()) {buf.jb.retval = (val); rsignal_post_longjmp(buf.tid, buf.jb.buffer);} else longjmp(buf.jb, val)

void rsignal_raise(int sig);
void rsignal_install_handler(int sig, __p_sig_fn_t fn);
void rsignal_deinstall_handler(int sig);
void rsignal_post_longjmp(unsigned long tid, intptr_t* builtin_jb);

//  rsignal_arm(): the thread's jump buffer is set, and its frame is
//  live: a signal raised from another thread may post a longjmp to it.
//
//  rsignal_disarm(): it may not, from here on; the virtual timer is
//  cancelled, and its callback, if one is running, has finished. call
//  before the frame that set the buffer goes on to anything a signal
//  mustn't unwind, and before it returns.
//
void rsignal_arm(void);
void rsignal_disarm(void);

#define ITIMER_VIRTUAL 1
struct itimerval {
	struct timeval it_value, it_interval;
};

int rsignal_setitimer(int type, struct itimerval *in, struct itimerval *out);

//  for tests: a handler is installed for [sig]; the virtual timer is armed
//
int rsignal_installed(int sig);
int rsignal_armed(void);

#endif//_RSIGNAL_H
