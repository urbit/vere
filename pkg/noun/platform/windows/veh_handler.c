#include "noun.h"
#include "rsignal.h"

c3_i
u3m_fault(void* adr_v, c3_i ser_i);

/* u3_windows_stack_guard(): reserve stack for the overflow handler.
**
**   POSIX runs the stack-overflow handler on a dedicated alternate stack
**   -- Sigstk, handed to libsigsegv by stackoverflow_install_handler().
**   windows has no equivalent: a vectored handler runs on the very stack
**   that just overflowed, with only whatever slack the guard page left.
**
**   that is about a page, and the handler has to get through
**   rsignal_raise(), _cm_signal_handle() and u3m_signal() to reach its
**   longjmp. if it runs out on the way, the process dies where it stands
**   and never reports dig: over.
**
**   NB: per-thread, and it can only be raised. called for the thread
**   that installs the handler.
*/
void
u3_windows_stack_guard(void)
{
  ULONG gar_u = 64 * 1024;

  if ( !SetThreadStackGuarantee(&gar_u) ) {
    fprintf(stderr, "boot: stack guarantee: win32 error %lu\r\n",
                    GetLastError());
  }
}

/* _windows_exception_filter: replaces libsigsegv on windows
   using vectored exception handling
 */
LONG WINAPI
_windows_exception_filter(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
  EXCEPTION_RECORD ExceptionRecord = *ExceptionInfo->ExceptionRecord;

  if (ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    ULONG_PTR kin_d = ExceptionRecord.ExceptionInformation[0];
    void*     adr_w = (void*)ExceptionRecord.ExceptionInformation[1];

    // ExceptionInformation[0] is 0 for a read, 1 for a write, 8 for a DEP
    // violation. a sparse loom faults on the first *read* of a reserved
    // page, so reads must be handled too.
    //
    if ((kin_d == 1) || (kin_d == 0)) {
      if (u3m_fault(adr_w, 1)) {
        return EXCEPTION_CONTINUE_EXECUTION;
      }
    }
  }

  if (ExceptionRecord.ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    rsignal_raise(SIGSTK);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}
