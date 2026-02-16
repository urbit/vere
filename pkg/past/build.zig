const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};
    const vere32 = b.option(bool, "vere32", "") orelse false;

    const pkg_past = b.addLibrary(.{
        .name = "past",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            pkg_past.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            pkg_past.addLibraryPath(macos_sdk.?.path("usr/lib"));
            pkg_past.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    const pkg_c3 = b.dependency("pkg_c3", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    pkg_past.linkLibC();

    pkg_past.linkLibrary(pkg_c3.artifact("c3"));
    pkg_past.linkLibrary(pkg_noun.artifact("noun"));
    pkg_past.linkLibrary(gmp.artifact("gmp"));

    var flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(&.{
        // "-pedantic",
        "-std=gnu23",
    });
    try flags.appendSlice(copts);

    const c_sources: []const []const u8 = if (vere32) &.{
        "v1.c",
        "v2.c",
        "v3.c",
        "v4.c",
        "v5.c",
        "migrate_v2.c",
        "migrate_v3.c",
        "migrate_v4.c",
        "migrate_v5.c",
    } else &.{
        "v5.c",
        "migrate_v6.c",
    };

    const headers: []const []const u8 = if (vere32) &.{
        "v1.h",
        "v2.h",
        "v3.h",
        "v4.h",
        "v5.h",
        "migrate.h",
    } else &.{
        "v5.h",
        "v6.h",
        "migrate.h",
    };

    pkg_past.addCSourceFiles(.{
        .root = b.path(""),
        .files = c_sources,
        .flags = flags.items,
    });

    for (headers) |h| pkg_past.installHeader(b.path(h), h);

    b.installArtifact(pkg_past);
}
