#include "noun.h"
#include "rsignal.h"
#include <windows.h>

int err_win_to_posix(DWORD winerr);

//  The current implementation of rsignal_ is single-threaded,
//  but it can be extended to multi-threaded by replacing these
//  static variables with a thread id-based hash map.
//
static __p_sig_fn_t   _fns[SIG_COUNT];
static volatile DWORD _tid;
static HANDLE _hvt;

//  a longjmp may be posted to the thread (see rsignal_arm()). read by
//  the raising thread only once the thread is stopped, so it can't
//  change between the check and the post
//
static volatile LONG  _liv;

void rsignal_install_handler(int sig, __p_sig_fn_t fn)
{
  if (sig < 0 || sig >= SIG_COUNT)
    return;

  DWORD newtid = GetCurrentThreadId();
  DWORD oldtid = InterlockedExchange((volatile LONG*)&_tid, newtid);
  if (oldtid != 0 && oldtid != newtid) {
    fprintf(stderr, "\r\nrsignal_install_handler: %lu -> %lu\r\n", oldtid, newtid);
    return;
  }

  __p_sig_fn_t oldfn = InterlockedExchangePointer((PVOID*)&_fns[sig], fn);
  if (fn != 0 && oldfn != 0 && oldfn != fn) {
    fprintf(stderr, "\r\nrsignal_install_handler: %p -> %p\r\n", oldfn, fn);
  }
}

void rsignal_deinstall_handler(int sig)
{
    rsignal_install_handler(sig, 0);
}

void rsignal_raise(int sig)
{
  if (sig < 0 || sig >= SIG_COUNT)
    return;

  __p_sig_fn_t oldfn = InterlockedExchangePointer((PVOID*)&_fns[sig], 0);
  if (oldfn == 0)
    return;

  if (_tid == GetCurrentThreadId()) {
    oldfn(sig);
    return;
  }

  HANDLE hthread = OpenThread(THREAD_ALL_ACCESS, FALSE, _tid);
  if (!hthread) {
    fprintf(stderr, "\r\nrsignal_raise: OpenThread(%lu): %lu\r\n", _tid, GetLastError());
    return;
  }

  if (SuspendThread(hthread) < 0) {
    fprintf(stderr, "\r\nrsignal_raise: SuspendThread(%lu): %lu\r\n", _tid, GetLastError());
    goto cleanup;
  }

  //  SuspendThread() only asks; GetThreadContext() waits until the
  //  thread has stopped. if it has disarmed by then, the handler taken
  //  above is one it has finished with: the signal is dropped, neither
  //  delivered nor left for the thread to raise itself
  //
  {
    CONTEXT context;
    context.ContextFlags = CONTEXT_CONTROL;

    if (!GetThreadContext(hthread, &context) || !_liv) {
      ResumeThread(hthread);
      goto cleanup;
    }
  }

  oldfn(sig);

  if (!ResumeThread(hthread)) {
    fprintf(stderr, "\r\nrsignal_raise: ResumeThread(%lu): %lu\r\n", _tid, GetLastError());

    // abort because the main thread is stuck
    abort();
  }

cleanup:
  CloseHandle(hthread);
}

int rsignal_installed(int sig)
{
  return (sig >= 0) && (sig < SIG_COUNT) && (0 != _fns[sig]);
}

int rsignal_armed(void)
{
  return NULL != _hvt;
}

void rsignal_arm(void)
{
  _liv = 1;
}

//  a raise takes the handler before it stops the thread, so removing
//  the handlers doesn't stop one already under way: its longjmp would be
//  posted after the thread had left the frame it jumps to, or into work
//  that runs with no handler installed. so: no deliveries from here
//  (rsignal_raise() checks once the thread is stopped), then wait for
//  the timer's callback, which now can't redirect this thread.
//
void rsignal_disarm(void)
{
  _liv = 0;

  if (_hvt != NULL) {
    DeleteTimerQueueTimer(NULL, _hvt, INVALID_HANDLE_VALUE);
    _hvt = NULL;
  }
}

static void _rsignal_vt_cb(PVOID param, BOOLEAN timedOut)
{
  rsignal_raise(SIGVTALRM);
}

int rsignal_setitimer(int type, struct itimerval *in, struct itimerval *out)
{
  if (in == 0) {
    errno = EFAULT;
    return -1;
  }

  if (type != ITIMER_VIRTUAL || out != 0) {
    errno = ENOTSUP;
    return -1;
  }

  if (_hvt != NULL) {
    DeleteTimerQueueTimer(NULL, _hvt, NULL);
    _hvt = NULL;
  }

  if (timerisset(&in->it_value) && !CreateTimerQueueTimer(&_hvt, NULL, _rsignal_vt_cb, NULL,
    in->it_value.tv_sec    * 1000 + in->it_value.tv_usec    / 1000,
    in->it_interval.tv_sec * 1000 + in->it_interval.tv_usec / 1000, 0))
  {
    errno = err_win_to_posix(GetLastError());
    return -1;
  } else {
    return 0;
  }
}

// direct import from ntdll.dll
extern DWORD64 __imp_KiUserExceptionDispatcher;

static void _rsignal_longjmp(intptr_t* builtin_jb)
{
  __builtin_longjmp((void**)builtin_jb, 1);
}

void rsignal_post_longjmp(DWORD tid, intptr_t* builtin_jb)
{
  HANDLE hthread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
  if (!hthread) {
    fprintf(stderr, "\r\nrsignal: OpenThread(%lu): %lu\r\n", tid, GetLastError());
    return;
  }

  CONTEXT context;
  context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  if (!GetThreadContext(hthread, &context)) {
    fprintf(stderr, "\r\nrsignal: GetThreadContext(%lu): %lu\r\n", tid, GetLastError());
    goto cleanup;
  }

  //  the thread is stopped now (GetThreadContext() waits for the
  //  suspension). if it has disarmed, its jump buffer is stale: drop
  //  the signal
  //
  if (!_liv) {
    goto cleanup;
  }

  //  see if the thread is currently handling a structured exception
  //  if so, let the handler (usually the libsigsegv handler) finish
  //  and set up the the signal to run at the exception resume point
  //  otherwise, passing a parameter to fn is completely unreliable
  //
  DWORD64 kibase;
  PRUNTIME_FUNCTION ki = RtlLookupFunctionEntry(__imp_KiUserExceptionDispatcher, &kibase, NULL);
  CONTEXT c = context;
  while (1)
  {
    DWORD64 base, frame;
    PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(c.Rip, &base, NULL);
    if (!f) break;
    if (f == ki)
    {
      //  KiUserExceptionDispatcher has a "bare" frame
      //  with $rsp pointing to the CONTEXT structure
      //
      ((PCONTEXT)c.Rsp)->Rip = (DWORD64)_rsignal_longjmp;
      ((PCONTEXT)c.Rsp)->Rcx = (DWORD64)builtin_jb;
      goto cleanup;
    }
    PVOID handler_data;
    RtlVirtualUnwind(0, base, c.Rip, f, &c, &handler_data, &frame, NULL);
  }

  context.Rip = (DWORD64)_rsignal_longjmp;
  context.Rcx = (DWORD64)builtin_jb;
  if (!SetThreadContext(hthread, &context)) {
    fprintf(stderr, "\r\nrsignal: SetThreadContext(%lu): %lu\r\n", tid, GetLastError());
    goto cleanup;
  }

cleanup:
  CloseHandle(hthread);
}
