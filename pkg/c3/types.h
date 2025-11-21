/// @file

#ifndef C3_TYPES_H
#define C3_TYPES_H

#include "portable.h"

  /** Integer typedefs.
  **/
    /* Canonical integers.
    */
      typedef size_t c3_z;
      typedef ssize_t c3_zs;
      typedef uint64_t c3_d;
      typedef int64_t c3_ds;
      typedef uint32_t c3_h;  // -> s/b 32-bit always
      typedef int32_t c3_hs;
      typedef uint16_t c3_s;
      typedef int16_t c3_ss;
      typedef uint8_t c3_y;   // byte
      typedef int8_t c3_ys;   // signed byte
      typedef uint8_t c3_b;   // bit

      typedef uint8_t c3_t;   // boolean
      typedef uint8_t c3_o;   // loobean
      typedef uint8_t c3_g;   // u3a_word_bits log
      typedef uint32_t c3_m;  // mote; also c3_l; LSB first a-z 4-char string.
      typedef uint32_t c3_h;  // fixed 32-bit
      #ifdef VERE64
        typedef uint64_t c3_l;  // little; 63-bit unsigned integer
        typedef uint64_t c3_w;  // word: noun-sized integer
        typedef int64_t c3_ws;
      #else
        typedef uint32_t c3_l;  // little; 31-bit unsigned integer
        typedef uint32_t c3_w;  // word: noun-sized integer
        typedef int32_t c3_ws;
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
      #define SCNc3_w  SCNu64
      #define PRIc3_w  PRIu64
      #define PRIc3_ws PRIi64
      #define PRIxc3_w PRIx64
      #define PRIXc3_w PRIX64
      #define PRIc3_h  PRIu32
      #define PRIc3_hs PRIi32
      #else
      #define SCNc3_w  SCNu32
      #define PRIc3_w  PRIu32
      #define PRIc3_ws PRIi32
      #define PRIxc3_w PRIx32
      #define PRIXc3_w PRIX32
      #define PRIc3_h  PRIu32
      #define PRIc3_hs PRIi32
      #endif

      #ifdef VERE64
      #define PRIc3_l  PRIu64
      #define PRIc3_ls PRIi64
      #define PRIxc3_l PRIx64
      #define PRIXc3_l PRIX64
      #define PRIc3_h PRIu32
      #define PRIc3_ls_new PRIi32
      #define PRIxc3_h PRIx32
      #define PRIXc3_h PRIX32
      #else
      #define PRIc3_l  PRIu32
      #define PRIc3_ls PRIi32
      #define PRIxc3_l PRIx32
      #define PRIXc3_l PRIX32
      #define PRIc3_h PRIu32
      #define PRIc3_ls_new PRIi32
      #define PRIxc3_h PRIx32
      #define PRIXc3_h PRIX32
      #endif

      #define PRIc3_m  PRIu32
      #define PRIc3_ms PRIi32
      #define PRIxc3_m PRIx32
      #define PRIXc3_m PRIX32

#endif /* ifndef C3_TYPES_H */
