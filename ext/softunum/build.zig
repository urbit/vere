const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addLibrary(.{
        .name = "softunum",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    const dep_c = b.dependency("softunum", .{
        .target = target,
        .optimize = optimize,
    });

    lib.addIncludePath(dep_c.path("include"));

    //  SoftUnum is pure-integer C -- no SoftFloat, no floating point.  Only the
    //  three per-width translation units compile; the algorithm headers
    //  (pcore.h, pwide.h, ptrans.h, pieee.h) are #included by them.
    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            "src/posit/p8.c",
            "src/posit/p16.c",
            "src/posit/p32.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("include/softunum.h"), "softunum.h");

    lib.linkLibC();
    b.installArtifact(lib);
}
