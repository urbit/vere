const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const h2o_c = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o = b.addStaticLibrary(.{
        .name = "h2o",
        .target = target,
        .optimize = optimize,
    });

    h2o.linkLibC();

    h2o.addIncludePath(h2o_c.path("include"));
    h2o.addIncludePath(h2o_c.path("include/h2o"));
    h2o.addIncludePath(h2o_c.path("include/h2o/socket"));

    h2o.addCSourceFiles(.{
        .root = h2o_c.path("lib"),
        .files = &.{
            "common/cache.c",
            "common/file.c",
            "common/filecache.c",
            "common/hostinfo.c",
            "common/http1client.c",
            "common/memcached.c",
            "common/memory.c",
            "common/multithread.c",
            "common/serverutil.c",
            "common/socket.c",
            "common/socketpool.c",
            "common/string.c",
            "common/time.c",
            "common/timeout.c",
            "common/url.c",
            "core/config.c",
            "core/configurator.c",
            "core/context.c",
            "core/headers.c",
            "core/logconf.c",
            "core/proxy.c",
            "core/request.c",
            "core/token.c",
            "core/util.c",
            "handler/access_log.c",
            "handler/chunked.c",
            "handler/compress.c",
            "handler/compress/gzip.c",
            "handler/configurator/access_log.c",
            "handler/configurator/compress.c",
            "handler/configurator/errordoc.c",
            "handler/configurator/expires.c",
            "handler/configurator/fastcgi.c",
            "handler/configurator/file.c",
            "handler/configurator/headers.c",
            "handler/configurator/headers_util.c",
            "handler/configurator/http2_debug_state.c",
            "handler/configurator/proxy.c",
            "handler/configurator/redirect.c",
            "handler/configurator/reproxy.c",
            "handler/configurator/status.c",
            "handler/configurator/throttle_resp.c",
            "handler/errordoc.c",
            "handler/expires.c",
            "handler/fastcgi.c",
            "handler/file.c",
            "handler/headers.c",
            "handler/headers_util.c",
            "handler/http2_debug_state.c",
            "handler/mimemap.c",
            "handler/proxy.c",
            "handler/redirect.c",
            "handler/reproxy.c",
            "handler/status.c",
            "handler/status/durations.c",
            "handler/status/events.c",
            "handler/status/requests.c",
            "handler/throttle_resp.c",
            "http1.c",
            "http2/cache_digests.c",
            "http2/casper.c",
            "http2/connection.c",
            "http2/frame.c",
            "http2/hpack.c",
            "http2/http2_debug_state.c",
            "http2/scheduler.c",
            "http2/stream.c",
            "tunnel.c",
        },
    });

    h2o.installHeader(h2o_c.path("include/h2o.h"), "h2o.h");

    b.installArtifact(h2o);
}
