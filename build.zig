const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const aes_siv = b.addStaticLibrary(.{
        .target = target,
        .name = "aes_siv",
        .link_libc = true,
        .zig_lib_dir = .{ .path = "external/aes_siv" },
        .main_mod_path = .{ .path = "pkg/aes_siv" },
        .optimize = optimize,
    });
    aes_siv.addCSourceFiles(&.{
        "pkg/aes_siv/aes.c",
        "pkg/aes_siv/config.h",
    }, &.{"-O3"});

    const urbit = b.addExecutable(.{
        .name = "urbit",
        .single_threaded = true,
        .target = target,
        .optimize = optimize,
    });
    urbit.addCSourceFiles(&.{
        "pkg/vere/main.c",
        // need ca_bundle, ivory, pace_hdr, version_hdr
    }, &.{"-O3"});
    b.installArtifact(urbit);
}
