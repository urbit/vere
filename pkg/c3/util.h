#ifndef C3_UTIL_H
#define C3_UTIL_H

#include <stdio.h>

#include "types.h"
#include "defs.h"

c3_w _c3_printcap_mem_w (FILE *fil_u, c3_w wor_w, const c3_c *cap_c);
c3_z _c3_printcap_mem_z(FILE *fil_u, c3_z byt_z, const c3_c *cap_c);


/*
  c3_print_mem_w(
      FILE * FILE  - file. must not be NULL,
      c3_w   WOR   - size in words,
    ? c3_c * q_CAP - caption,
  )

  Print word-sized quantity, WOR, in typical format:
  [Caption: ][GiB|MiB|KiB|B]/QUANTITY
*/
#define   c3_print_mem_w( ... )                 \
  c3_print_mem_w_0( __VA_ARGS__, "" )
#define c3_print_mem_w_0( FILE, WOR, q_CAP, ... )       \
  ((q_CAP[0])                                           \
   ? _c3_printcap_mem_w((FILE), (WOR), q_CAP ": ")      \
   : _c3_printcap_mem_w((FILE), (WOR), ""))


/*
  c3_print_mem_z(
      FILE * FILE  - file. must not be NULL,
      c3_w   BYT   - size in bytes,
    ? c3_c * q_CAP - caption,
  )

  Print byte-sized quantity, BYT like c3_print_mem_w
*/
#define   c3_print_mem_z( ... )                 \
  c3_print_mem_z_0( __VA_ARGS__, "" )
#define c3_print_mem_z_0( FILE, BYT, q_CAP, ... )       \
  ((q_CAP[0])                                           \
   ? _c3_printcap_mem_z((FILE), (BYT), q_CAP ": ")      \
   : _c3_printcap_mem_z((FILE), (BYT), ""))


/*
  c3_print_mem_w(
      FILE * FILE  - file may be NULL,
      c3_w   WOR   - size in words,
    ? c3_c * q_CAP - caption,
  )

  Just like c3_print_mem_w with the exception that FILE may be null. In which
  case, this is a NOP.
*/
#define   c3_maid_w( ... )                      \
  c3_maid_w_0( __VA_ARGS__, "" )
#define c3_maid_w_0( FILE, WOR, ... )           \
  ((0 == (FILE))                                \
   ? (WOR)                                      \
   : c3_print_mem_w( FILE, WOR, __VA_ARGS__ ))


/*
  c3_print_memdiff_w(
      FILE * FILE  - file,
      c3_w   WOR0  - size in words,
      c3_w   WOR1  - size in words,
    ? c3_c * q_CAP - caption,
  )

  Prints absolute value of WOR0 - WOR1 using c3_print_mem_w
*/
#define c3_print_memdiff_w( ... )               \
  c3_print_memdiff_w_0( __VA_ARGS__, "" )

#define c3_print_memdiff_w_0( FILE, WOR0, WOR1, ... )   \
  c3_print_mem_w(FILE, ((0+WOR0) > (0+WOR1) ? WOR0 - WOR1 : WOR1 - WOR0), __VA_ARGS__ )

#endif /* ifndef C3_UTIL_H */
