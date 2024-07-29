const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "natpmp",
        .target = target,
        .optimize = optimize,
    });
    lib.addIncludePath(b.path("include"));
    lib.addCSourceFiles(.{
        .files = &.{
            "natpmp.c",
            "getgateway.c",
        },
    });
    lib.linkLibC();
    lib.installHeader(b.path("natpmp.h"), "natpmp.h");
    lib.installHeader(b.path("getgateway.h"), "getgateway.h");
    lib.installHeader(b.path("natpmp_declspec.h"), "natpmp_declspec.h");
    b.installArtifact(lib);
}
