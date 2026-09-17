/// @file

#ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H
#define NOUN_PLATFORM_DARWIN_RSIGNAL_H

#define rsignal_jmpbuf                 sigjmp_buf
#define rsignal_setjmp(buf)            sigsetjmp((buf), 1)
#define rsignal_longjmp                siglongjmp
#define rsignal_install_handler        signal
#define rsignal_deinstall_handler(sig) signal((sig), SIG_IGN)
#define rsignal_setitimer              setitimer

#include <signal.h>

/* rsignal_block()/rsignal_unblock(): hold the signals whose handlers
** longjmp (SIGINT, SIGTERM, SIGVTALRM) across a critical section.
**
**   the kernel queues a blocked signal and delivers it at unblock, so a
**   handler never runs between the two calls.  see u3m_crit_enter().
*/
static inline void
rsignal_block(void)
{
  sigset_t set_u;
  sigemptyset(&set_u);
  sigaddset(&set_u, SIGINT);
  sigaddset(&set_u, SIGTERM);
  sigaddset(&set_u, SIGVTALRM);
  sigprocmask(SIG_BLOCK, &set_u, 0);
}

static inline void
rsignal_unblock(void)
{
  sigset_t set_u;
  sigemptyset(&set_u);
  sigaddset(&set_u, SIGINT);
  sigaddset(&set_u, SIGTERM);
  sigaddset(&set_u, SIGVTALRM);
  sigprocmask(SIG_UNBLOCK, &set_u, 0);
}

#endif /* ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H */
