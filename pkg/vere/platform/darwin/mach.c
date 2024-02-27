#include "log.h"

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/exception.h>
#include <mach/task.h>


// lldb does not listen to BSD signals, it only listens to Mach exceptions.
// The Mach exception EXC_BAD_ACCESS corresponds to SIGSEGV, but lldb has
// problems converting between them because of a longstanding macOS kernel bug.
// This means that without this workaround we cannot debug our binaries with
// lldb. The first segfault we hit causes an infinite loop in lldb no matter
// how many times you try to continue. This workaround is implemented in projects
// such as the Go runtime.
// See https://bugs.llvm.org/show_bug.cgi?id=22868#c1 for more details.

void darwin_register_mach_exception_handler() {
  kern_return_t kr = task_set_exception_ports(
    mach_task_self(),
    EXC_MASK_BAD_ACCESS | EXC_MASK_ARITHMETIC, // SIGSEGV, SIGFPE
    MACH_PORT_NULL,
    EXCEPTION_STATE_IDENTITY,
    MACHINE_THREAD_STATE);
  if ( KERN_SUCCESS != kr) {
    u3l_log("mono_runtime_install_handlers: task_set_exception_ports failed");
  }
}
