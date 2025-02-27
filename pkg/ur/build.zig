const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};

    const pkg_ur = b.addStaticLibrary(.{
        .name = "ur",
        .target = target,
        .optimize = optimize,
    });

    const murmur3 = b.dependency("murmur3", .{
        .target = target,
        .optimize = optimize,
    });

    pkg_ur.linkLibC();
    pkg_ur.linkLibrary(murmur3.artifact("murmur3"));

    pkg_ur.addIncludePath(b.path(""));
    pkg_ur.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{
            "bitstream.c",
            "hashcons.c",
            "serial.c",
        },
        .flags = copts,
    });

    pkg_ur.installHeader(b.path("bitstream.h"), "ur/bitstream.h");
    pkg_ur.installHeader(b.path("defs.h"), "ur/defs.h");
    pkg_ur.installHeader(b.path("hashcons.h"), "ur/hashcons.h");
    pkg_ur.installHeader(b.path("serial.h"), "ur/serial.h");
    pkg_ur.installHeader(b.path("ur.h"), "ur/ur.h");

    b.installArtifact(pkg_ur);
}
