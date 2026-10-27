const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addLibrary(.{
        .name = "pdjson",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });
    const no_lto = b.option(bool, "no_lto", "") orelse blk: {
        std.debug.print("{s}: 'no_lto' option not found\n",
        .{std.fs.path.basename(b.build_root.path.?)});
        break :blk target.result.os.tag == .macos;
    };
    lib.lto = if (optimize != .Debug and !no_lto) .full else null;

    lib.linkLibC();

    lib.addIncludePath(b.path("vendor"));

    lib.addCSourceFiles(.{
        .root = b.path("vendor"),
        .files = &.{"pdjson.c"},
        .flags = &.{
            "-fno-sanitize=all",
            "-std=c99",
            "-pedantic",
            "-Wall",
            "-Wextra",
            "-Wno-missing-field-initializers",
        },
    });

    lib.installHeader(b.path("vendor/pdjson.h"), "pdjson.h");

    b.installArtifact(lib);
}
