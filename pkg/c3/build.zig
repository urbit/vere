const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};

    const pkg_c3 = b.addStaticLibrary(.{
        .name = "c3",
        .target = target,
        .optimize = optimize,
    });

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            pkg_c3.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            pkg_c3.addLibraryPath(macos_sdk.?.path("usr/lib"));
            pkg_c3.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    pkg_c3.linkLibC();

    pkg_c3.addIncludePath(b.path(""));

    pkg_c3.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{"defs.c"},
        .flags = copts,
    });

    pkg_c3.installHeader(b.path("c3.h"), "c3/c3.h");
    pkg_c3.installHeader(b.path("defs.h"), "c3/defs.h");
    pkg_c3.installHeader(b.path("motes.h"), "c3/motes.h");
    pkg_c3.installHeader(b.path("portable.h"), "c3/portable.h");
    pkg_c3.installHeader(b.path("types.h"), "c3/types.h");

    b.installArtifact(pkg_c3);
}
