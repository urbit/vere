#ifndef U3_VERSION_H
#define U3_VERSION_H

/* VORTEX
 */

typedef c3_w       u3v_version;

#define U3V_VER1   1
#define U3V_VER2   2
#define U3V_VER3   3
#define U3V_VERLAT U3V_VER3

/* EVENTS
 */

typedef c3_w       u3e_version;

#define U3E_VER1   1
#define U3E_VERLAT U3E_VER1

/* DISK FORMAT
 *
 * synchronized between:
 * - u3_disk->ver_w
 * - log/<0iN>/epoc.txt
 * - version key in META table of data.mdb files
 */

#define U3D_VER1   1         // <= vere-v2
#define U3D_VER2   2         // migrating to >= vere-v3
#define U3D_VER3   3         // >= vere-v3
#define U3D_VERLAT U3D_VER3

#endif /* ifndef U3_VERSION_H */
