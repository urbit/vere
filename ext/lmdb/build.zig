const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lmdb_c = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.addStaticLibrary(.{
        .name = "lmdb",
        .target = target,
        .optimize = optimize,
    });

    lmdb.linkLibC();

    lmdb.addIncludePath(lmdb_c.path("libraries/liblmdb"));

    lmdb.addCSourceFiles(.{
        .root = lmdb_c.path("libraries/liblmdb"),
        .files = &.{
            "mdb.c",
            "midl.c",
        },
        .flags = &.{
            "-pthread",
            "-O2",
            "-g",
            "-W",
            "-Wall",
            "-Wno-unused-parameter",
            "-Wbad-function-cast",
            "-Wuninitialized",
        },
    });

    lmdb.installHeader(lmdb_c.path("libraries/liblmdb/lmdb.h"), "lmdb/lmdb.h");

    b.installArtifact(lmdb);
}
