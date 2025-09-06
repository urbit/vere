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
  if (ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
      ExceptionRecord.ExceptionInformation[0] == 1 &&
      u3m_fault((void*)ExceptionRecord.ExceptionInformation[1], 1))
    {
      return EXCEPTION_CONTINUE_EXECUTION;
    }

  if (ExceptionRecord.ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    rsignal_raise(SIGSTK);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}
