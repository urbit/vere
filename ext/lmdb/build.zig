const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lmdb_c = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.addLibrary(.{
        .name = "lmdb",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });
    const no_lto = b.option(bool, "no_lto", "") orelse @panic("no_lto flag missing in config struct");
    lmdb.lto = if (optimize != .Debug and !no_lto) .full else null;

    lmdb.linkLibC();

    lmdb.addIncludePath(lmdb_c.path("libraries/liblmdb"));

    var flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer flags.deinit();

    try flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-pthread",
        "-O2",
        "-g",
        "-W",
        "-Wall",
        "-Wno-unused-parameter",
        "-Wbad-function-cast",
        "-Wuninitialized",
    });

    lmdb.addCSourceFiles(.{
        .root = lmdb_c.path("libraries/liblmdb"),
        .files = &.{ "midl.c", "mdb.c" },
        .flags = flags.items,
    });

    lmdb.installHeader(lmdb_c.path("libraries/liblmdb/lmdb.h"), "lmdb/lmdb.h");

    b.installArtifact(lmdb);
}
