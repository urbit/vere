const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const murmur3 = b.addLibrary(.{
        .name = "murmur3",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });
    const no_lto = b.option(bool, "no_lto", "") orelse blk: {
        std.debug.print("{s}: 'no_lto' option not found\n",
        .{std.fs.path.basename(b.build_root.path.?)});
        break :blk target.result.os.tag == .macos;
    };
    murmur3.lto = if (optimize != .Debug and !no_lto) .full else null;

    murmur3.linkLibC();

    murmur3.addIncludePath(b.path("."));

    const common_flags = [_][]const u8{
        "-fno-sanitize=all",
        "-O3",
        "-Wall",
    };

    const mac_flags = common_flags ++ [_][]const u8{
        "-fPIC",
        "-c",
    };

    murmur3.addCSourceFiles(.{
        .root = b.path("vendor"),
        .files = &.{"murmur3.c"},
        .flags = if (t.os.tag == .macos) &mac_flags else &common_flags,
    });

    murmur3.installHeader(b.path("vendor/murmur3.h"), "murmur3.h");

    b.installArtifact(murmur3);
}
