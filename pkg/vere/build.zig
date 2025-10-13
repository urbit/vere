const std = @import("std");

const wslay_version = "1.1.1";
const wslay_version_define =
    std.fmt.comptimePrint("-DWSLAY_VERSION=\"{s}\"", .{wslay_version});

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};
    const pace = b.option([]const u8, "pace", "") orelse
        @panic("Missing required option: pace");
    const version = b.option([]const u8, "version", "") orelse
        @panic("Missing required option: version");

    const pkg_vere = b.addStaticLibrary(.{
        .name = "vere",
        .target = target,
        .optimize = optimize,
    });

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            pkg_vere.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            pkg_vere.addLibraryPath(macos_sdk.?.path("usr/lib"));
            pkg_vere.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    const pkg_c3 = b.dependency("pkg_c3", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ent = b.dependency("pkg_ent", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ur = b.dependency("pkg_ur", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const avahi = b.dependency("avahi", .{
        .target = target,
        .optimize = optimize,
    });

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const wslay = b.dependency("wslay", .{
        .target = target,
        .optimize = optimize,
    });

    const libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const natpmp = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const urcrypt = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
    });

    const pace_h = b.addWriteFile("pace.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(b.fmt(
            \\#ifndef URBIT_PACE_H
            \\#define URBIT_PACE_H
            \\#define U3_VERE_PACE "{s}"
            \\#endif
            \\
        , .{pace}));

        break :blk try output.toOwnedSlice();
    });

    const version_h = b.addWriteFile("version.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(b.fmt(
            \\#ifndef URBIT_VERSION_H
            \\#define URBIT_VERSION_H
            \\#define URBIT_VERSION "{s}"
            \\#endif
            \\
        , .{version}));

        break :blk try output.toOwnedSlice();
    });

    pkg_vere.addIncludePath(pace_h.getDirectory());
    pkg_vere.addIncludePath(version_h.getDirectory());
    pkg_vere.addIncludePath(b.path(""));
    pkg_vere.addIncludePath(b.path("ivory"));
    pkg_vere.addIncludePath(b.path("ca_bundle"));
    pkg_vere.addIncludePath(wslay.path("lib/includes"));

    if (t.os.tag == .linux) {
        pkg_vere.linkLibrary(avahi.artifact("dns-sd"));
    }
    pkg_vere.linkLibrary(natpmp.artifact("natpmp"));
    pkg_vere.linkLibrary(curl.artifact("curl"));
    pkg_vere.linkLibrary(gmp.artifact("gmp"));
    pkg_vere.linkLibrary(h2o.artifact("h2o"));
    pkg_vere.linkLibrary(wslay.artifact("wslay"));
    pkg_vere.linkLibrary(libuv.artifact("libuv"));
    pkg_vere.linkLibrary(lmdb.artifact("lmdb"));
    pkg_vere.linkLibrary(openssl.artifact("ssl"));
    pkg_vere.linkLibrary(urcrypt.artifact("urcrypt"));
    pkg_vere.linkLibrary(zlib.artifact("z"));
    pkg_vere.linkLibrary(pkg_c3.artifact("c3"));
    pkg_vere.linkLibrary(pkg_ent.artifact("ent"));
    pkg_vere.linkLibrary(pkg_ur.artifact("ur"));
    pkg_vere.linkLibrary(pkg_noun.artifact("noun"));
    pkg_vere.linkLibC();

    var files = std.ArrayList([]const u8).init(b.allocator);
    defer files.deinit();
    try files.appendSlice(&c_source_files);
    if (t.os.tag == .macos) {
        try files.appendSlice(&.{
            "platform/darwin/daemon.c",
            "platform/darwin/ptty.c",
            "platform/darwin/mach.c",
        });
    }
    if (t.os.tag == .linux) {
        try files.appendSlice(&.{
            "platform/linux/daemon.c",
            "platform/linux/ptty.c",
        });
    }

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(&.{
        "-std=gnu23",
        wslay_version_define,
    });
    try flags.appendSlice(copts);

    pkg_vere.addCSourceFiles(.{
        .root = b.path(""),
        .files = files.items,
        .flags = flags.items,
    });

    for (install_headers) |h| pkg_vere.installHeader(b.path(h), h);
    pkg_vere.installHeader(b.path("ivory/ivory.h"), "ivory.h");
    pkg_vere.installHeader(b.path("ca_bundle/ca_bundle.h"), "ca_bundle.h");
    pkg_vere.installHeader(pace_h.getDirectory().path(b, "pace.h"), "pace.h");
    pkg_vere.installHeader(version_h.getDirectory().path(b, "version.h"), "version.h");

    b.installArtifact(pkg_vere);
}

const c_source_files = [_][]const u8{
    "auto.c",
    "ca_bundle/ca_bundle.c",
    "dawn.c",
    "db/lmdb.c",
    "disk.c",
    "foil.c",
    "io/ames.c",
    "io/ames/stun.c",
    "io/behn.c",
    "io/conn.c",
    "io/cttp.c",
    "io/fore.c",
    "io/hind.c",
    "io/http.c",
    "io/lick.c",
    "io/lss.c",
    "io/mesa.c",
    "io/mesa/bitset.c",
    "io/mesa/pact.c",
    "io/term.c",
    "io/unix.c",
    "ivory/ivory.c",
    "king.c",
    "lord.c",
    "mars.c",
    "mdns.c",
    "melt.c",
    "newt.c",
    "pier.c",
    "save.c",
    "serf.c",
    "time.c",
    "ward.c",
};

const install_headers = [_][]const u8{
    "db/lmdb.h",
    "dns_sd.h",
    "io/ames/stun.h",
    "io/lss.h",
    "io/mesa/bitset.h",
    "io/mesa/mesa.h",
    "io/serial.h",
    "arena.h",
    "mars.h",
    "mdns.h",
    "serf.h",
    "vere.h",
    "verstable.h",
};
