const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};

    const pkg_c3 = b.addLibrary(.{ .name = "c3", .root_module = b.createModule(.{ .target = target, .optimize = optimize }) });
    const no_lto = b.option(bool, "no_lto", "") orelse blk: {
        std.debug.print("{s}: 'no_lto' option not found\n",
        .{std.fs.path.basename(b.build_root.path.?)});
        break :blk target.result.os.tag == .macos;
    };
    pkg_c3.lto = if (optimize != .Debug and !no_lto) .full else null;

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

    if (t.os.tag == .windows) {
        pkg_c3.addIncludePath(b.path("platform/windows"));
        pkg_c3.installHeadersDirectory(b.path("platform/windows"), "", .{});
        pkg_c3.addCSourceFiles(.{
            .root = b.path(""),
            .files = &.{"platform/windows/compat.c"},
            .flags = copts,
        });
    }

    pkg_c3.installHeader(b.path("c3.h"), "c3/c3.h");
    pkg_c3.installHeader(b.path("defs.h"), "c3/defs.h");
    pkg_c3.installHeader(b.path("motes.h"), "c3/motes.h");
    pkg_c3.installHeader(b.path("portable.h"), "c3/portable.h");
    pkg_c3.installHeader(b.path("types.h"), "c3/types.h");

    b.installArtifact(pkg_c3);
}
