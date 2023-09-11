/// @file

#ifndef U3_ERROR_H
#define U3_ERROR_H

#include "manage.h"

/* Assert.  Good to capture.

   TODO: determine which u3_assert calls can rather call c3_dessert, i.e. in
   public releases, which calls to u3_assert should abort and which should
   no-op? If the latter, is the assert useful inter-development to validate
   conditions we might accidentally break or not useful at all?
*/

#if defined(ASAN_ENABLED) && defined(__clang__)
# define u3_assert(x)                       \
    do {                                    \
      if (!(x)) {                           \
        u3m_bail(c3__oops);                 \
        abort();                            \
      }                                     \
    } while(0)
#else
# define u3_assert(x)                       \
    do {                                    \
      if (!(x)) {                           \
        fflush(stderr);                     \
        fprintf(stderr, "\rAssertion '%s' " \
                "failed in %s:%d\r\n",      \
                #x, __FILE__, __LINE__);    \
        u3m_bail(c3__oops);                 \
        abort();                            \
      }                                     \
    } while(0)
#endif /* if defined(ASAN_ENABLED) && defined(__clang__) */

#endif /* ifndef U3_ERROR_H */
