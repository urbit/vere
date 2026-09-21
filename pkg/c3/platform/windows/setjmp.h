#ifndef _SETJMP_H
#define _SETJMP_H

//  mingw would otherwise typedef jmp_buf a second time if its own
//  <setjmp.h> is ever reached by a path that bypasses our include dir.
//
#define _INC_SETJMP

//  msvcrt setjmp/longjmp are broken on 64-bit systems, use gcc builtins.
//
//  this shadows the system <setjmp.h> for every translation unit that
//  links c3, which is all of them.  it has to: jmp_buf sits in the road
//  (u3a_road_esc in allocate.h), and the road is loom layout, so libnoun,
//  libvere and the test binaries must all agree on its size and alignment.
//  mingw's 64-bit jmp_buf is _CRT_ALIGN(16) while this one is 8-aligned,
//  which moved every road field after esc -- and every u3v_home field
//  after rod_u -- by eight bytes between libraries.
//
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
