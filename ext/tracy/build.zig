const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const tracy_src = b.dependency("tracy", .{
        .target = target,
        .optimize = optimize,
    });

    const tracy = b.addStaticLibrary(.{
        .name = "tracy",
        .target = target,
        .optimize = optimize,
    });

    tracy.linkLibC();
    tracy.linkLibCpp();

    // Add Tracy include path
    tracy.addIncludePath(tracy_src.path("public"));

    // Add Tracy C++ client source
    tracy.addCSourceFiles(.{
        .root = tracy_src.path("public"),
        .files = &.{"TracyClient.cpp"},
        .flags = &.{
            "-DTRACY_ENABLE",
            "-fno-sanitize=undefined",
            "-D_WIN32_WINNT=0x601", // Windows compatibility
            "-std=c++11",
        },
    });

    // Platform-specific libraries
    const t = target.result;
    if (t.os.tag == .linux) {
        tracy.linkSystemLibrary("pthread");
        tracy.linkSystemLibrary("dl");
    } else if (t.os.tag.isDarwin()) {
        // macOS might need additional frameworks
    } else if (t.os.tag == .windows) {
        tracy.linkSystemLibrary("ws2_32");
        tracy.linkSystemLibrary("dbghelp");
    }

    // Install Tracy headers for C API - install the entire public directory structure
    tracy.installHeadersDirectory(tracy_src.path("public"), "", .{});

    b.installArtifact(tracy);
}