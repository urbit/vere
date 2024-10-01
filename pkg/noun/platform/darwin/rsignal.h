/// @file

#ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H
#define NOUN_PLATFORM_DARWIN_RSIGNAL_H

#define rsignal_jmpbuf                 sigjmp_buf
#define rsignal_setjmp(buf)            sigsetjmp((buf), 1)
#define rsignal_longjmp                siglongjmp
#define rsignal_install_handler        signal
#define rsignal_deinstall_handler(sig) signal((sig), SIG_IGN)
#define rsignal_setitimer              setitimer
#define rsignal_getitimer              getitimer

#endif /* ifndef NOUN_PLATFORM_DARWIN_RSIGNAL_H */
