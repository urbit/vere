const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const uv_c = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const uv = b.addStaticLibrary(.{
        .name = "libuv",
        .target = target,
        .optimize = optimize,
    });

    uv.linkLibC();

    uv.addIncludePath(uv_c.path("src"));
    uv.addIncludePath(uv_c.path("include"));

    var uv_flags = std.ArrayList([]const u8).init(b.allocator);
    defer uv_flags.deinit();

    try uv_flags.appendSlice(&.{
        "-fno-sanitize=all",
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
            "-U_DEBUG",
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

    if (t.os.tag == .windows) {
        uv.addCSourceFiles(.{
            .files = &.{"patches/libuv/src/win/tty.c"},
            .flags = uv_flags.items,
        });
        uv.addIncludePath(uv_c.path("src/win"));
    }

    uv.installHeadersDirectory(uv_c.path("include"), "", .{});

    if (t.os.tag == .windows) {
        uv.linkSystemLibrary("ole32"); // CoTaskMemFree
        uv.linkSystemLibrary("dbghelp"); // MiniDumpWriteDump, SymGetOptions, SymSetOptions
        uv.linkSystemLibrary("userenv"); // GetUserProfileDirectoryW
        uv.linkSystemLibrary("iphlpapi"); // GetAdaptersAddresses, ConvertInterface*
    }

    b.installArtifact(uv);
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
    "unix/proctitle.c",
    "unix/random-devurandom.c",
    "unix/signal.c",
    "unix/stream.c",
    "unix/tcp.c",
    "unix/thread.c",
    "unix/tty.c",
    "unix/udp.c",
};

const uv_srcs_linux = uv_srcs_unix ++ [_][]const u8{
    "unix/linux.c",
    "unix/procfs-exepath.c",
    "unix/random-getrandom.c",
    "unix/random-sysctl-linux.c",
};

const uv_srcs_macos = uv_srcs_unix ++ [_][]const u8{
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
    "win/udp.c",
    "win/util.c",
    "win/winapi.c",
    "win/winsock.c",
};
