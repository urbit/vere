/// @file

#ifndef C3_H
#define C3_H

#include "defs.h"
#include "motes.h"
#include "portable.h"
#include "types.h"

inline double
timespec_diff(struct timespec const* befor,
              struct timespec const* after)
{
  if (after->tv_sec < befor->tv_sec)
    return -timespec_diff(befor, after);
  return
    (after->tv_sec - befor->tv_sec) * 1 /* seconds */
    + (after->tv_nsec - befor->tv_nsec) * 1E-9;
}

#endif /* ifndef C3_H */
