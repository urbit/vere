const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const dep_c = b.dependency("aes_siv", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "aes_siv",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibrary(openssl.artifact("ssl"));

    const config_h = b.addConfigHeader(
        .{.style = .blank, .include_path = "config.h",},
        .{}
    );
    lib.addConfigHeader(config_h);
    lib.addIncludePath(dep_c.path(""));
    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            "aes_siv.c",
        },
    });

    lib.installHeader(dep_c.path("aes_siv.h"), "aes_siv.h");

    lib.linkLibC();
    b.installArtifact(lib);
}
