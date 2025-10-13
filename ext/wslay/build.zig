const std = @import("std");

const wslay_version = "1.1.1";

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const wslay_src = b.dependency("wslay", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "wslay",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(wslay_src.path("lib"));
    lib.addIncludePath(wslay_src.path("lib/includes"));

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();

    try flags.append("-fno-sanitize=all");
    try flags.append(b.fmt("-DWSLAY_VERSION=\\\"{s}\\\"", .{wslay_version}));

    if (t.os.tag == .windows) {
        try flags.append("-DHAVE_WINSOCK2_H=1");
    } else {
        try flags.append("-DHAVE_ARPA_INET_H=1");
        try flags.append("-DHAVE_NETINET_IN_H=1");
    }

    if (t.cpu.arch.endian() == .big) {
        try flags.append("-DWORDS_BIGENDIAN=1");
    }

    lib.addCSourceFiles(.{
        .root = wslay_src.path("lib"),
        .files = &.{
            "wslay_event.c",
            "wslay_frame.c",
            "wslay_net.c",
            "wslay_queue.c",
            "wslay_stack.c",
        },
        .flags = flags.items,
    });

    lib.installHeadersDirectory(wslay_src.path("lib/includes"), "", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(lib);
}
