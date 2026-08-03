const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addLibrary(.{
        .name = "pdjson",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

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
