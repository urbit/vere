/// @file
///
/// Zero-cost wrappers around the Tracy C client (ext/tracy, TracyC.h).
///
/// All macros expand to nothing unless the build defines TRACY_ENABLE
/// (i.e. `zig build ... -Dtracy=true`), so instrumented call sites add no
/// instructions and no behavior change to ordinary release/debug builds.
///
/// Only translation units in a Tracy-wired library may include this header:
/// pkg_noun is wired (pkg/noun/build.zig links the Tracy client and adds its
/// include path when -DTRACY_ENABLE is present), as are the test/benchmark
/// executables. pkg_c3 / pkg_vere libraries are NOT wired, so do not include
/// this from those translation units under -Dtracy.
///
/// Usage:
///
///     #include "tracy.h"
///     void f(void) {
///       u3_tc_zone(zone);            // open a scoped zone named after f()
///       ... hot work ...
///       u3_tc_zone_end(zone);        // close it on every exit path
///     }
///
/// Use u3_tc_zone_named(z, "label") to give an explicit name, u3_tc_plot to
/// graph a numeric series (e.g. loom high-water-mark), u3_tc_frame to mark a
/// frame boundary (one per benchmark iteration/group), and u3_tc_msg to drop a
/// labelled marker into the timeline.

#ifndef U3_NOUN_TRACY_H
#define U3_NOUN_TRACY_H

#ifdef TRACY_ENABLE

#include "tracy/TracyC.h"

  //  open a zone whose name is the enclosing function
  //
#  define u3_tc_zone(ctx)               TracyCZone(ctx, 1)

  //  open a zone with an explicit static string name
  //
#  define u3_tc_zone_named(ctx, name)               \
     TracyCZoneN(ctx, name, 1)

  //  attach a numeric value to an open zone
  //
#  define u3_tc_zone_value(ctx, val)    TracyCZoneValue(ctx, (uint64_t)(val))

  //  close a previously-opened zone
  //
#  define u3_tc_zone_end(ctx)           TracyCZoneEnd(ctx)

  //  mark a frame boundary (default, or named for a sub-stream)
  //
#  define u3_tc_frame                   TracyCFrameMark
#  define u3_tc_frame_named(name)       TracyCFrameMarkNamed(name)

  //  plot a numeric series sample
  //
#  define u3_tc_plot(name, val)         TracyCPlot(name, (double)(val))

  //  drop a labelled marker (static string literal)
  //
#  define u3_tc_msg(txt)                TracyCMessageL(txt)

#else  //  !TRACY_ENABLE  — all no-ops

#  define u3_tc_zone(ctx)               ((void)0)
#  define u3_tc_zone_named(ctx, name)   ((void)0)
#  define u3_tc_zone_value(ctx, val)    ((void)0)
#  define u3_tc_zone_end(ctx)           ((void)0)
#  define u3_tc_frame                   ((void)0)
#  define u3_tc_frame_named(name)       ((void)0)
#  define u3_tc_plot(name, val)         ((void)0)
#  define u3_tc_msg(txt)                ((void)0)

#endif  //  TRACY_ENABLE

#endif  //  U3_NOUN_TRACY_H
