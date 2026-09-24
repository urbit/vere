const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const t = target.result;
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addLibrary(.{
        .name = "natpmp",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });
    const no_lto = b.option(bool, "no_lto", "") orelse blk: {
        std.debug.print("{s}: 'no_lto' option not found\n",
        .{std.fs.path.basename(b.build_root.path.?)});
        break :blk target.result.os.tag == .macos;
    };
    lib.lto = if (optimize != .Debug and !no_lto) .full else null;

    const dep_c = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });

    lib.addIncludePath(dep_c.path("include"));

    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            "natpmp.c",
            "getgateway.c",
            "wingettimeofday.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("natpmp.h"), "natpmp.h");
    lib.installHeader(dep_c.path("getgateway.h"), "getgateway.h");
    lib.installHeader(dep_c.path("natpmp_declspec.h"), "natpmp_declspec.h");

    if (t.os.tag == .windows and t.cpu.arch == .x86_64) {
        lib.root_module.addCMacro("WIN32", "");
        lib.root_module.addCMacro("NATPMP_STATICLIB", "");
        lib.root_module.addCMacro("ENABLE_STRNATPMPERR", "");
    }

    lib.linkLibC();
    b.installArtifact(lib);
}
