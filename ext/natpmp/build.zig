const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "natpmp",
        .target = target,
        .optimize = optimize,
    });

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
        },
    });

    lib.installHeader(dep_c.path("natpmp.h"), "natpmp.h");
    lib.installHeader(dep_c.path("getgateway.h"), "getgateway.h");
    lib.installHeader(dep_c.path("natpmp_declspec.h"), "natpmp_declspec.h");

    lib.linkLibC();
    b.installArtifact(lib);
}
