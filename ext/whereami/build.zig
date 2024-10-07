const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const dep_c = b.dependency("whereami", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "whereami",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("src"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("src"),
        .files = &.{"whereami.c"},
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("src/whereami.h"), "whereami.h");

    b.installArtifact(lib);
}
