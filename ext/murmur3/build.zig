const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const murmur3_c = b.dependency("murmur3", .{
        .target = target,
        .optimize = optimize,
    });

    const murmur3 = b.addStaticLibrary(.{
        .name = "murmur3",
        .target = target,
        .optimize = optimize,
    });

    murmur3.linkLibC();

    murmur3.addIncludePath(murmur3_c.path("."));

    const common_flags = [_][]const u8{ "-O3", "-Wall" };

    const mac_flags = common_flags ++ [_][]const u8{
        "-fPIC",
        "-c",
    };

    murmur3.addCSourceFiles(.{
        .root = murmur3_c.path("."),
        .files = &.{"murmur3.c"},
        .flags = if (t.os.tag == .macos) &mac_flags else &common_flags,
    });

    murmur3.installHeader(murmur3_c.path("murmur3.h"), "murmur3.h");

    b.installArtifact(murmur3);
}
