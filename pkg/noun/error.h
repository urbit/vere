/// @file

#ifndef U3_ERROR_H
#define U3_ERROR_H

#include "manage.h"

/// Bail with `c3_oops` if the given condition is false.
#define u3_assert(condition)                                                   \
  do {                                                                         \
    if ( !(condition) ) {                                                      \
      fprintf(stderr,                                                          \
              "\rAssertion '%s' failed in %s:%d\r\n",                          \
              #condition,                                                      \
              __FILE__,                                                        \
              __LINE__);                                                       \
      u3m_bail(c3__oops);                                                      \
    }                                                                          \
  } while ( 0 )

#endif /* ifndef U3_ERROR_H */
