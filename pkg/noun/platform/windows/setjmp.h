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
#define setjmp(buf) (windows_setjmp((void**)(buf.buffer)) ? (buf.retval) : 0)

#define u3_prep_setjmp() asm volatile("" ::: "rbx", "rbp", "r12", "r13", "r14", "r15", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15")

__attribute__((naked,returns_twice,noinline)) int windows_setjmp(void** buf);

#endif// _SETJMP_H
