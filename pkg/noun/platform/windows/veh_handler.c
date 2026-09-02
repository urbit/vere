#include "noun.h"
#include "rsignal.h"

c3_i
u3m_fault(void* adr_v, c3_i ser_i);

/* _windows_exception_filter: replaces libsigsegv on windows
   using vectored exception handling
 */
LONG WINAPI
_windows_exception_filter(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
  EXCEPTION_RECORD ExceptionRecord = *ExceptionInfo->ExceptionRecord;

  if (ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    ULONG_PTR kin_d = ExceptionRecord.ExceptionInformation[0];
    c3_w*     adr_w = (c3_w*)ExceptionRecord.ExceptionInformation[1];

    // ExceptionInformation[0] is 0 for a read, 1 for a write, 8 for a DEP
    // violation. a sparse loom faults on the first *read* of a reserved
    // page, so reads must be handled too -- but only within the loom, so
    // that stray reads elsewhere keep crashing the way they always have.
    //
    if ((kin_d == 1) ||
        (kin_d == 0 && adr_w >= u3_Loom && adr_w < (u3_Loom + u3C.wor_i)))
      {
        if (u3m_fault((void*)adr_w, 1))
          {
            return EXCEPTION_CONTINUE_EXECUTION;
          }
      }
  }

  if (ExceptionRecord.ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    rsignal_raise(SIGSTK);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}
