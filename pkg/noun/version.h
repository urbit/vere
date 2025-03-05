#ifndef U3_VERSION_H
#define U3_VERSION_H

/* VORTEX
 */

typedef c3_w_tmp       u3v_version;

#define U3V_VER1   1
#define U3V_VER2   2
#define U3V_VER3   3
#define U3V_VER4   4
#define U3V_VERLAT U3V_VER4

/* PATCHES
 */

typedef c3_w_tmp       u3e_version;

#define U3P_VER1   1
#define U3P_VERLAT U3P_VER1

/* DISK FORMAT
 *
 * synchronized between:
 * - u3_disk->ver_w
 * - version key in META table of data.mdb files
 */

#define U3D_VER1   1         // <= vere-v2
#define U3D_VER2   2         // migrating to >= vere-v3
#define U3D_VER3   3         // >= vere-v3
#define U3D_VERLAT U3D_VER3

/* EPOCH SYSTEM
*/

#define U3E_VER1   1
#define U3E_VERLAT U3E_VER1

#endif /* ifndef U3_VERSION_H */
