const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(&.{
        "-pedantic",
        "-std=gnu99",
    });
    try flags.appendSlice(copts);

    const pkg_ent = b.addStaticLibrary(.{
        .name = "ent",
        .target = target,
        .optimize = optimize,
    });

    pkg_ent.linkLibC();

    pkg_ent.addIncludePath(b.path(""));

    pkg_ent.addCSourceFiles(.{
        .root = b.path(""),
        .files = &.{"ent.c"},
        .flags = flags.items,
    });

    pkg_ent.installHeader(b.path("ent.h"), "ent/ent.h");

    b.installArtifact(pkg_ent);
}
