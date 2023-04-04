#ifndef C3_UTIL_H
#define C3_UTIL_H

extern void *
main_frame_addr;

void minidump(void);

/*
  Designed only to be called in main. Could theoretically be called anywhere
  you want the tail of your backtrace to be.
*/
__attribute__((always_inline)) inline
void
minidump_init(void)
{
  /*
     it is necessary to store the frame address of main so that when we produce
     a backtrace, we avoid overshooting __builtin_frame_address(N) with some
     value of N that exceeeds main -- inevitably leading to a SIGSEGV when we
     attempt to dereference the return address of libc_start_main_stage2. FUBAR!
  */
  main_frame_addr = __builtin_frame_address(0);
}

#endif  /* ifndef C3_UTIL_H */
