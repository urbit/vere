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

    const aes_siv = b.dependency("aes_siv", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(aes_siv.artifact("aes_siv"));

    const natpmp = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(natpmp.artifact("natpmp"));

    const softfloat = b.dependency("softfloat", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(softfloat.artifact("softfloat"));

    b.installArtifact(exe);
}
