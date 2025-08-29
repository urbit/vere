#ifndef TRACY_WRAPPER_H
#define TRACY_WRAPPER_H

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#else
// Define empty macros when Tracy is disabled for zero-overhead
#define TracyCZone(ctx, active)
#define TracyCZoneEnd(ctx)
#define TracyCFrameMark
#define TracyCZoneN(ctx, name, active)
#define TracyCZoneText(ctx, txt, size)
#define TracyCZoneValue(ctx, value)
#define TracyCZoneColor(ctx, color)
#define TracyCMessage(txt, size)
#define TracyCMessageL(txt)
#define TracyCMessageC(txt, size, color)
#define TracyCMessageLC(txt, color)
#define TracyCAppInfo(txt, size)
#define TracyCPlot(name, val)
#define TracyCPlotF(name, val)
#define TracyCPlotI(name, val)
#endif

// Convenience macros for common profiling patterns
#ifdef TRACY_ENABLE
#define TRACY_ZONE() TracyCZone(__tracy_ctx, 1); 
#define TRACY_ZONE_END() TracyCZoneEnd(__tracy_ctx);
#define TRACY_ZONE_SCOPED() TracyCZone(__tracy_ctx, 1); defer { TracyCZoneEnd(__tracy_ctx); }
#define TRACY_ZONE_NAMED(name) TracyCZoneN(__tracy_ctx, name, 1);
#define TRACY_FRAME_MARK() TracyCFrameMark;
#define TRACY_MESSAGE(msg) TracyCMessageL(msg);
#define TRACY_PLOT(name, value) TracyCPlot(name, value);
#else
#define TRACY_ZONE()
#define TRACY_ZONE_END()
#define TRACY_ZONE_SCOPED()
#define TRACY_ZONE_NAMED(name)
#define TRACY_FRAME_MARK()
#define TRACY_MESSAGE(msg)
#define TRACY_PLOT(name, value)
#endif

#endif // TRACY_WRAPPER_H