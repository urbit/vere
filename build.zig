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

    const avahi = b.dependency("avahi", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(avahi.artifact("dns-sd"));

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

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(curl.artifact("curl"));

    b.installArtifact(exe);
}
