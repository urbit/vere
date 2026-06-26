const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const luv_c = b.dependency("luv", .{ .target = target, .optimize = optimize });
    const lua_c = b.dependency("lua", .{ .target = target, .optimize = optimize });
    const libuv = b.dependency("libuv", .{ .target = target, .optimize = optimize });

    const luv = b.addLibrary(.{
        .name = "luv",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    luv.linkLibC();
    //  libuv provides <uv.h> (and the symbols, at final link)
    luv.linkLibrary(libuv.artifact("libuv"));

    //  luv's own headers + unity .c includes
    luv.addIncludePath(luv_c.path("src"));
    //  compat-5.3 shim that luv.c includes
    luv.addIncludePath(luv_c.path("deps/lua-compat-5.3/c-api"));
    //  raw lua headers (<lua.h>, <lauxlib.h>, ...)
    luv.addIncludePath(lua_c.path("src"));

    var flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-O2",
        "-g",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
    });

    //  luv is a unity build: src/luv.c #includes every other src/*.c
    luv.addCSourceFiles(.{
        .root = luv_c.path("src"),
        .files = &.{"luv.c"},
        .flags = flags.items,
    });

    luv.installHeader(luv_c.path("src/luv.h"), "luv/luv.h");

    b.installArtifact(luv);
}
