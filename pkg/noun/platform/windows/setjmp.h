#ifndef _SETJMP_H
#define _SETJMP_H

// msvcrt setjmp/longjmp are broken on 64-bit systems, use gcc builtins
typedef struct jmp_buf {
  intptr_t buffer[5];
  int retval;
} jmp_buf;

#define _setjmp setjmp
#define _longjmp longjmp
#define longjmp(buf, val) {(buf).retval = (val); __builtin_longjmp((void**)((buf).buffer), 1);}
#define setjmp(buf) (__builtin_setjmp((void**)(buf.buffer)) ? (buf.retval) : 0)

#endif// _SETJMP_H
