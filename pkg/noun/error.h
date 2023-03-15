/// @file

#ifndef U3_ERROR_H
#define U3_ERROR_H

#include "manage.h"

#define u3_assert(condition)                                                   \
  do {                                                                         \
    if ( !(condition) ) {                                                      \
      u3m_bail(c3_oops);                                                       \
    }                                                                          \
  } while ( 0 )

#endif /* ifndef U3_ERROR_H */
