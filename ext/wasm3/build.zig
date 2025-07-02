const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const softfloat = b.dependency("softfloat", .{
        .target = target,
        .optimize = optimize,
    });

    const wasm3_c = b.dependency("wasm3", .{
        .target = target,
        .optimize = optimize,
    });

    const wasm3 = b.addStaticLibrary(.{
        .name = "wasm3",
        .target = target,
        .optimize = optimize,
    });

    wasm3.linkLibC();

    wasm3.addIncludePath(wasm3_c.path("source/"));

    const common_flags = [_][]const u8{
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wparentheses",
        "-Wundef",
        "-Wpointer-arith",
        "-Wstrict-aliasing=2",
        "-Werror=implicit-function-declaration",
        "-fno-sanitize=all",
        // "-fsanitize=undefined",
        // "-fno-sanitize-trap=undefined",
    };

    const mac_flags = common_flags ++ [_][]const u8{
        "-fPIC",
        "-c",
    };

    wasm3.addCSourceFiles(.{
        .root = wasm3_c.path("source/"),
        .files = &.{
          "m3_bind.c",
          "m3_code.c",
          "m3_compile.c",
          "m3_core.c",
          "m3_emit.c",
          "m3_env.c",
          "m3_exec.c",
          "m3_function.c",
          "m3_info.c",
          "m3_module.c",
          "m3_parse.c",
          "m3_validate.c",
          "m3_rewrite.c",
          "m3_resume.c",
        },
        .flags = if (t.os.tag == .macos) &mac_flags else &common_flags,
    });
 
    wasm3.installHeader(wasm3_c.path("source/m3_config_platforms.h"), "m3_config_platforms.h");
    wasm3.installHeader(wasm3_c.path("source/m3_bind.h"), "m3_bind.h");
    wasm3.installHeader(wasm3_c.path("source/m3_code.h"), "m3_code.h");
    wasm3.installHeader(wasm3_c.path("source/m3_compile.h"), "m3_compile.h");
    wasm3.installHeader(wasm3_c.path("source/m3_config.h"), "m3_config.h");
    wasm3.installHeader(wasm3_c.path("source/m3_core.h"), "m3_core.h");
    wasm3.installHeader(wasm3_c.path("source/m3_emit.h"), "m3_emit.h");
    wasm3.installHeader(wasm3_c.path("source/m3_env.h"), "m3_env.h");
    wasm3.installHeader(wasm3_c.path("source/m3_exception.h"), "m3_exception.h");
    wasm3.installHeader(wasm3_c.path("source/m3_exec.h"), "m3_exec.h");
    wasm3.installHeader(wasm3_c.path("source/m3_exec_defs.h"), "m3_exec_defs.h");
    wasm3.installHeader(wasm3_c.path("source/m3_function.h"), "m3_function.h");
    wasm3.installHeader(wasm3_c.path("source/m3_info.h"), "m3_info.h");
    wasm3.installHeader(wasm3_c.path("source/m3_math_utils.h"), "m3_math_utils.h");
    wasm3.installHeader(wasm3_c.path("source/wasm3.h"), "wasm3.h");
    wasm3.installHeader(wasm3_c.path("source/wasm3_defs.h"), "wasm3_defs.h");
    wasm3.installHeader(wasm3_c.path("source/m3_validate.h"), "m3_validate.h");
    wasm3.installHeader(wasm3_c.path("source/m3_rewrite.h"), "m3_rewrite.h");
    wasm3.installHeader(wasm3_c.path("source/m3_resume.h"), "m3_resume.h");

    wasm3.linkLibrary(softfloat.artifact("softfloat"));

    b.installArtifact(wasm3);
}
