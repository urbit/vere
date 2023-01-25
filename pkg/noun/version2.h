#ifndef U3_VERSION2_H           /* ;;: temporary name until name conflict in new
                                   vere repo is resolved -- possibly by
                                   removing version.h altogether (the one that
                                   defines URBIT_VERSION in BUILD.bazel
                                   genrule version_hdr) and supplying as a -D
                                   argument via "defines" rule

                                   https://bazel.build/reference/be/c-cpp
                                   https://bazel.build/reference/be/make-variables#custom_variables
                                   https://bazel.build/reference/be/make-variables
                                */
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
