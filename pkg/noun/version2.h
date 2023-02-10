#ifndef U3_VERSION2_H
#define U3_VERSION2_H

/* VORTEX
 */

typedef enum u3v_version {
  U3V_VER1   = 1,
  /* 1 -> 2: 1 bit pointer compression to enable 8G loom */
  U3V_VER2   = 2,
  U3V_VERLAT = U3V_VER2,
} u3v_version;

/* EVENTS
 */

typedef enum u3e_version {
  U3E_VER1   = 1,
  U3E_VERLAT = U3E_VER1,
} u3e_version;

#endif /* ifndef U3_VERSION2_H */
