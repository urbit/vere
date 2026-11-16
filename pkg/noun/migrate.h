/// @file

#ifndef U3_NOUN_MIGRATE_H
#define U3_NOUN_MIGRATE_H

#include "c3/c3.h"
#include "vortex.h"

    /* u3_load_{h,d}(): locate u3v_home_{h,d} in the mapped 32-bit /
    ** 64-bit image.
    */
      void
      u3_load_h(c3_z wor_i);

      void
      u3_load_d(c3_z wor_i);

    /* u3_migrate_h(): migrate the loaded 64-bit snapshot into the native
    ** 32-bit loom.  Called from disk.c on a 32-bit vere reading a 64-bit
    ** snapshot.
    */
      void
      u3_migrate_h(c3_d eve_d);

    /* u3_migrate_d(): migrate the loaded 32-bit snapshot into the native
    ** 64-bit loom.  Called from disk.c on a 64-bit vere reading a 32-bit
    ** snapshot.
    */
      void
      u3_migrate_d(c3_d eve_d);

    /* u3_migrate_v6(): migrate the version 5 image live in the loom to
    ** version 6, which appended the SKA state to the road, where the
    ** image has zeros: create its tables (and the version 5 tables an
    ** older image lacks), rebuild the registrations for SKA from the
    ** cold state, and set the version.  A step of u3_disk_migrate_home().
    */
      void
      u3_migrate_v6(c3_d eve_d);

#endif
