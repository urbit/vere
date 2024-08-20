const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const patches = b.dependency("patches", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const uv_c = b.dependency("uv", .{
        .target = target,
        .optimize = optimize,
    });

    const uv = b.addStaticLibrary(.{
        .name = "uv",
        .target = target,
        .optimize = optimize,
    });

    if (t.os.tag == .windows) {
        uv.linkSystemLibrary("psapi");
        uv.linkSystemLibrary("user32");
        uv.linkSystemLibrary("advapi32");
        uv.linkSystemLibrary("iphlpapi");
        uv.linkSystemLibrary("userenv");
        uv.linkSystemLibrary("ws2_32");
    }
    if (t.os.tag == .linux) {
        uv.linkSystemLibrary("pthread");
    }
    uv.linkLibC();

    uv.addIncludePath(uv_c.path("src"));
    uv.addIncludePath(uv_c.path("include"));

    var uv_flags = std.ArrayList([]const u8).init(b.allocator);
    defer uv_flags.deinit();

    try uv_flags.appendSlice(&.{
        "-DHAVE_STDIO_H=1",
        "-DHAVE_STDLIB_H=1",
        "-DHAVE_STRING_H=1",
        "-DHAVE_INTTYPES_H=1",
        "-DHAVE_STDINT_H=1",
        "-DHAVE_STRINGS_H=1",
        "-DHAVE_SYS_STAT_H=1",
        "-DHAVE_SYS_TYPES_H=1",
        "-DHAVE_UNISTD_H=1",
        "-DHAVE_DLFCN_H=1",
        "-DHAVE_PTHREAD_PRIO_INHERIT=1",
        "-DSTDC_HEADERS=1",
        "-DSUPPORT_ATTRIBUTE_VISIBILITY_DEFAULT=1",
        "-DSUPPORT_FLAG_VISIBILITY=1",
    });

    if (t.os.tag != .windows) {
        try uv_flags.appendSlice(&.{
            "-D_FILE_OFFSET_BITS=64",
            "-D_LARGEFILE_SOURCE",
        });
    }

    _ = switch (t.os.tag) {
        .macos => try uv_flags.appendSlice(&.{
            "-D_DARWIN_UNLIMITED_SELECT=1",
            "-D_DARWIN_USE_64_BIT_INODE=1",
        }),
        .linux => try uv_flags.appendSlice(&.{
            "-D_GNU_SOURCE",
            "-D_POSIX_C_SOURCE=200112",
        }),
        .windows => try uv_flags.appendSlice(&.{
            "-DWIN32_LEAN_AND_MEAN",
            "-D_WIN32_WINNT=0x0602",
        }),
        else => null,
    };

    uv.addCSourceFiles(.{
        .root = uv_c.path("src"),
        .files = switch (t.os.tag) {
            .macos => &uv_srcs_macos,
            .linux => &uv_srcs_linux,
            .windows => &uv_srcs_windows,
            else => &.{},
        },
        .flags = uv_flags.items,
    });

    uv.installHeadersDirectory(uv_c.path("include"), "", .{});

    const h2o_c = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const cloexec = b.addStaticLibrary(.{
        .name = "cloexec",
        .target = target,
        .optimize = optimize,
    });
    cloexec.linkLibC();
    cloexec.addIncludePath(h2o_c.path("deps/cloexec"));
    cloexec.addCSourceFiles(.{
        .root = h2o_c.path("deps/cloexec"),
        .files = &.{"cloexec.c"},
    });
    cloexec.installHeader(h2o_c.path("deps/cloexec/cloexec.h"), "cloexec.h");

    const sse2neon_c = b.dependency("sse2neon", .{
        .target = target,
        .optimize = optimize,
    });

    const klib = b.addStaticLibrary(.{
        .name = "klib",
        .target = target,
        .optimize = optimize,
    });
    klib.linkLibC();
    klib.addIncludePath(h2o_c.path("deps/klib"));
    if (t.cpu.arch == .aarch64) {
        klib.addIncludePath(sse2neon_c.path("."));
    }
    klib.addCSourceFiles(.{
        .root = h2o_c.path("deps/klib"),
        .files = &.{
            "bgzf.c",
            "khmm.c",
            "kmath.c",
            "knetfile.c",
            "knhx.c",
            "kopen.c",
            "ksa.c",
            "kson.c",
            "kstring.c",
            // "ksw.c",
            "kthread.c",
            "kurl.c",
        },
    });
    klib.addCSourceFiles(.{
        .root = patches.path("h2o-2.2.6/deps/klib"),
        .files = &.{"ksw.c"},
        .flags = &.{
            if (t.cpu.arch == .aarch64)
                "-DURBIT_RUNTIME_CPU_AARCH64"
            else
                "",
        },
    });
    klib.installHeadersDirectory(h2o_c.path("deps/klib"), "", .{
        .include_extensions = &.{".h"},
    });

    const libgkc = b.addStaticLibrary(.{
        .name = "libgkc",
        .target = target,
        .optimize = optimize,
    });
    libgkc.linkLibC();
    libgkc.addIncludePath(h2o_c.path("deps/libgkc"));
    libgkc.addCSourceFiles(.{
        .root = h2o_c.path("deps/libgkc"),
        .files = &.{"gkc.c"},
    });
    libgkc.installHeader(h2o_c.path("deps/libgkc/gkc.h"), "gkc.h");

    const libyrmcds = b.addStaticLibrary(.{
        .name = "libyrmcds",
        .target = target,
        .optimize = optimize,
    });
    libyrmcds.linkLibC();
    libyrmcds.addIncludePath(h2o_c.path("deps/libyrmcds"));
    libyrmcds.addCSourceFiles(.{
        .root = h2o_c.path("deps/libyrmcds"),
        .files = &.{
            "close.c",
            "connect.c",
            "counter.c",
            "recv.c",
            "send.c",
            "send_text.c",
            "set_compression.c",
            "socket.c",
            "strerror.c",
            "text_mode.c",
            // "yc-cnt.c",
            // "yc.c",
        },
        .flags = &.{
            "-Wall",
            "-Wconversion",
            "-gdwarf-3",
            "-O2",
        },
    });
    libyrmcds.installHeadersDirectory(h2o_c.path("deps/libyrmcds"), "", .{
        .include_extensions = &.{".h"},
    });

    const h2o = b.addStaticLibrary(.{
        .name = "h2o",
        .target = target,
        .optimize = optimize,
    });

    h2o.linkLibrary(openssl.artifact("ssl"));
    h2o.linkLibrary(uv);
    h2o.linkLibrary(cloexec);
    h2o.linkLibrary(klib);
    h2o.linkLibrary(libgkc);
    h2o.linkLibrary(libyrmcds);
    h2o.linkLibC();

    h2o.addIncludePath(h2o_c.path("include"));
    h2o.addIncludePath(h2o_c.path("include/h2o"));
    h2o.addIncludePath(h2o_c.path("include/h2o/socket"));

    h2o.addIncludePath(h2o_c.path("deps/golombset"));

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

const uv_srcs = [_][]const u8{
    "fs-poll.c",
    "idna.c",
    "inet.c",
    "random.c",
    "strscpy.c",
    "strtok.c",
    "threadpool.c",
    "timer.c",
    "uv-common.c",
    "uv-data-getter-setters.c",
    "version.c",
};

const uv_srcs_unix = uv_srcs ++ [_][]const u8{
    "unix/async.c",
    "unix/core.c",
    "unix/dl.c",
    "unix/fs.c",
    "unix/getaddrinfo.c",
    "unix/getnameinfo.c",
    "unix/loop-watcher.c",
    "unix/loop.c",
    "unix/pipe.c",
    "unix/poll.c",
    "unix/process.c",
    "unix/random-devurandom.c",
    "unix/signal.c",
    "unix/stream.c",
    "unix/tcp.c",
    "unix/thread.c",
    "unix/tty.c",
    "unix/udp.c",
};

const uv_srcs_linux = uv_srcs_unix ++ [_][]const u8{
    "unix/linux-core.c",
    "unix/linux-inotify.c",
    "unix/linux-syscalls.c",
    "unix/procfs-exepath.c",
    "unix/random-getrandom.c",
    "unix/random-sysctl-linux.c",
    "unix/epoll.c",
};

const uv_srcs_macos = uv_srcs_unix ++ [_][]const u8{
    "unix/proctitle.c",
    "unix/bsd-ifaddrs.c",
    "unix/kqueue.c",
    "unix/random-getentropy.c",
    "unix/darwin-proctitle.c",
    "unix/darwin.c",
    "unix/fsevents.c",
};

const uv_srcs_windows = uv_srcs ++ [_][]const u8{
    "win/async.c",
    "win/core.c",
    "win/detect-wakeup.c",
    "win/dl.c",
    "win/error.c",
    "win/fs.c",
    "win/fs-event.c",
    "win/getaddrinfo.c",
    "win/getnameinfo.c",
    "win/handle.c",
    "win/loop-watcher.c",
    "win/pipe.c",
    "win/thread.c",
    "win/poll.c",
    "win/process.c",
    "win/process-stdio.c",
    "win/signal.c",
    "win/snprintf.c",
    "win/stream.c",
    "win/tcp.c",
    "win/tty.c",
    "win/udp.c",
    "win/util.c",
    "win/winapi.c",
    "win/winsock.c",
};
