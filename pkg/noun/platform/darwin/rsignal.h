/// @file

#ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H
#define NOUN_PLATFORM_DARWIN_RSIGNAL_H

#define rsignal_jmpbuf                 sigjmp_buf
#define rsignal_setjmp(buf)            sigsetjmp((buf), 1)
#define rsignal_longjmp                siglongjmp

//  signals are delivered on the thread they interrupt, and a cancelled
//  timer raises none: nothing to do (see platform/windows/rsignal.h)
//
#define rsignal_arm()
#define rsignal_disarm()
#define rsignal_install_handler        signal
#define rsignal_deinstall_handler(sig) signal((sig), SIG_IGN)
#define rsignal_setitimer              setitimer

#endif /* ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H */
