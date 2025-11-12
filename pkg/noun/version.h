#ifndef U3_VERSION_H
#define U3_VERSION_H

/*  loom layout
 */
typedef c3_d       u3v_version;

#define U3V_VER1   (u3v_version)1  //  1.0
#define U3V_VER2   (u3v_version)2  //  2.0:    pointer compression
#define U3V_VER3   (u3v_version)3  //  3.0-rc: persistent memoization
#define U3V_VER4   (u3v_version)4  //  3.0:    bytecode alignment
#define U3V_VER5   (u3v_version)5  //  ??      palloc
#define U3V_VERLAT U3V_VER5

/*  bytecode semantics (within u3v_version)
 */
typedef c3_w       u3n_version;

#define U3N_VER1   (u3n_version)0  // zero-indexedfor backcompat
#define U3N_VER2   (u3n_version)1
#define U3N_VERLAT U3N_VER2

/*  snapshot patch format
 */
typedef c3_w       u3e_version;

#define U3P_VER2   (u3e_version)2  //  top-level checksum added
#define U3P_VERLAT U3P_VER2

/*  top-level event log format
 */
#define U3D_VER1   1               //  <= 2.0
#define U3D_VER2   2               //  migration to 3.0 in-progress
#define U3D_VER3   3               //  3.0 (epoch system)
#define U3D_VERLAT U3D_VER3

/*  epoch layout
*/
#define U3E_VER1   1               //  north+south.bin
#define U3E_VER2   2               //  image.bin
#define U3E_VERLAT U3E_VER2

#endif /* ifndef U3_VERSION_H */
