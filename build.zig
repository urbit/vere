const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "urbit",
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFile(.{ .file = b.path("zig_test.c") });

    b.installArtifact(exe);
}
