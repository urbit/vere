#ifndef _SETJMP_H
#define _SETJMP_H

//  also mingw's own guard: this header is installed for dependents, so
//  it must keep the system <setjmp.h> from typedef'ing jmp_buf again.
#define _INC_SETJMP

// msvcrt setjmp/longjmp are broken on 64-bit systems, use gcc builtins
typedef struct jmp_buf {
  intptr_t buffer[5];
  int retval;
} jmp_buf;

#define _setjmp setjmp
#define _longjmp longjmp
#define longjmp(buf, val) {(buf).retval = (val); __builtin_longjmp((void**)((buf).buffer), 1);}
#define setjmp(buf) (windows_setjmp((void**)(buf.buffer)) ? (buf.retval) : 0)

__attribute__((naked,returns_twice,preserve_none)) int windows_setjmp(void** buf);

#endif// _SETJMP_H
