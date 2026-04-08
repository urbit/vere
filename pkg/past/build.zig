const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};
    const vere32: bool = b.option(bool, "vere32", "") orelse false;

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

    const c_source_files: []const []const u8 = if (vere32)
        &.{
            "32/v1.c",
            "32/v2.c",
            "32/v3.c",
            "32/v4.c",
            "64/v5.c",
            "32/migrate_v2.c",
            "32/migrate_v3.c",
            "32/migrate_v4.c",
            "32/migrate_v5.c",
            "32/migrate.c",
        }
    else
        &.{
            "32/v5.c",
            "64/migrate.c",
        };

    const install_headers: []const []const u8 = if (vere32)
        &.{
            "32/v1.h",
            "32/v2.h",
            "32/v3.h",
            "32/v4.h",
            "32/v5.h",
            "64/v5.h",
            "migrate.h",
        }
    else
        &.{
            "32/v5.h",
            "64/v5.h",
            "migrate.h",
        };

    pkg_past.addCSourceFiles(.{
        .root = b.path(""),
        .files = c_source_files,
        .flags = flags.items,
    });

    for (install_headers) |h| pkg_past.installHeader(b.path(h), h);

    b.installArtifact(pkg_past);
}
