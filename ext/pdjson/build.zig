const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const dep_c = b.dependency("pdjson", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "pdjson",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("."));

    lib.addCSourceFiles(.{
        .root = dep_c.path("."),
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

    lib.installHeader(dep_c.path("pdjson.h"), "pdjson.h");

    b.installArtifact(lib);
}
