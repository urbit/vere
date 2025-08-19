/// @file

#ifndef C3_TYPES_H
#define C3_TYPES_H

#include "portable.h"

  /** Integer typedefs.
  **/
    /* Canonical integers.
    **
    ** Migration process for c3_w variables:
    ** 1. All c3_w variables have been renamed to c3_w_tmp.
    ** 2. Review c3_w_tmp variables:
    **   a. If s/b static 32-bit  -> rename to c3_w_new.
    **   b. If s/b arch-dependent -> rename to c3_n.
    ** 3. Review c3_w_new variables:
    **   a. Ensure correctness.
    **   b. Rename to c3_{???} (our new, static uint32_t type, name pending).
    */
      typedef size_t c3_z;
      typedef ssize_t c3_zs;
      typedef uint64_t c3_d;
      typedef int64_t c3_ds;
      typedef uint32_t c3_w_tmp;  // -> still needs review
      typedef uint32_t c3_w_new;  // -> s/b 32-bit always
      typedef int32_t c3_ws_tmp;
      typedef int32_t c3_ws_new;
      typedef uint16_t c3_s;
      typedef int16_t c3_ss;
      typedef uint8_t c3_y;   // byte
      typedef int8_t c3_ys;   // signed byte
      typedef uint8_t c3_b;   // bit

      typedef uint8_t c3_t;   // boolean
      typedef uint8_t c3_o;   // loobean
      typedef uint8_t c3_g;   // u3a_note_bits log
      typedef uint32_t c3_m;  // mote; also c3_l; LSB first a-z 4-char string.
      typedef uint32_t c3_l_new;  // -> s/b 32-bit always
      #ifdef VERE64
        typedef uint32_t c3_l_tmp;  // little; 31-bit unsigned integer
        typedef uint64_t c3_l;  // little; 31-bit unsigned integer
        typedef uint64_t c3_n;  // note: noun-sized integer
        typedef int64_t c3_ns;
      #else
        typedef uint32_t c3_l_tmp;  // little; 31-bit unsigned integer
        typedef uint32_t c3_l;  // little; 31-bit unsigned integer
        typedef uint32_t c3_n;  // note: noun-sized integer
        typedef int32_t c3_ns;
      #endif

    /* Deprecated integers.
    */
      typedef char      c3_c;      // does not match int8_t or uint8_t
      typedef int       c3_i;      // int - really bad
      typedef uintptr_t c3_p;      // pointer-length uint - really really bad
      typedef intptr_t c3_ps;      // pointer-length int - really really bad

      /* Print specifiers
      */

      /* c3_z */
      #define PRIc3_z  "zu"      /* unsigned dec */
      #define PRIc3_zs "zd"      /*   signed dec */
      #define PRIxc3_z "zx"      /* unsigned hex */
      #define PRIXc3_z "zX"      /* unsigned HEX */

      /* c3_d */
      #define PRIc3_d  PRIu64
      #define PRIc3_ds PRIi64
      #define PRIxc3_d PRIx64
      #define PRIXc3_d PRIX64

      /* c3_w_tmp */
      #define PRIc3_w_tmp  PRIu32
      #define PRIc3_ws_tmp PRIi32
      #define PRIxc3_w_tmp PRIx32
      #define PRIXc3_w_tmp PRIX32

      /* c3_s */
      #define PRIc3_s  PRIu16
      #define PRIc3_ss PRIi16
      #define PRIxc3_s PRIx16
      #define PRIXc3_s PRIX16

      /* c3_y */
      #define PRIc3_y  PRIu8
      #define PRIc3_ys PRIi8
      #define PRIxc3_y PRIx8
      #define PRIXc3_y PRIX8

      /* c3_b */
      #define PRIc3_b  PRIu8
      #define PRIxc3_b PRIx8
      #define PRIXc3_b PRIX8

      #ifdef VERE64
      #define SCNc3_n  SCNu64
      #define PRIc3_n  PRIu64
      #define PRIc3_ns PRIi64
      #define PRIxc3_n PRIx64
      #define PRIXc3_n PRIX64
      #define PRIc3_w_new  PRIu32
      #define PRIc3_ws_new PRIi32
      #else
      #define SCNc3_n  SCNu32
      #define PRIc3_n  PRIu32
      #define PRIc3_ns PRIi32
      #define PRIxc3_n PRIx32
      #define PRIXc3_n PRIX32
      #define PRIc3_w_new  PRIu32
      #define PRIc3_ws_new PRIi32
      #endif

      #ifdef VERE64
      #define PRIc3_l  PRIu64
      #define PRIc3_ls PRIi64
      #define PRIxc3_l PRIx64
      #define PRIXc3_l PRIX64
      #define PRIc3_l_tmp PRIu32
      #define PRIc3_ls_tmp PRIi32
      #define PRIxc3_l_tmp PRIx32
      #define PRIXc3_l_tmp PRIX32
      #else
      #define PRIc3_l  PRIu32
      #define PRIc3_ls PRIi32
      #define PRIxc3_l PRIx32
      #define PRIXc3_l PRIX32
      #endif

      #define PRIc3_m  PRIu32
      #define PRIc3_ms PRIi32
      #define PRIxc3_m PRIx32
      #define PRIXc3_m PRIX32

#endif /* ifndef C3_TYPES_H */
