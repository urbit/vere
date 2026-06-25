const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lua_c = b.dependency("lua", .{
        .target = target,
        .optimize = optimize,
    });

    const lua = b.addLibrary(.{
        .name = "lua",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    lua.linkLibC();

    lua.addIncludePath(lua_c.path("src"));

    const t = target.result;

    var flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer flags.deinit();

    try flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-O2",
        "-g",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
    });

    //  LUA_USE_POSIX enables the POSIX feature set (gmtime_r, popen, etc.)
    //  without LUA_USE_DLOPEN, so embedded Lua cannot load native C modules.
    //  This keeps the surface small; we choose which std libs to open in C.
    //
    if (t.os.tag == .linux or t.os.tag == .macos) {
        try flags.append("-DLUA_USE_POSIX");
    }

    //  Lua core + standard library. The CLI mains lua.c and luac.c are
    //  intentionally excluded -- we embed the library only.
    //
    lua.addCSourceFiles(.{
        .root = lua_c.path("src"),
        .files = &.{
            // core VM
            "lapi.c",    "lcode.c",   "lctype.c",  "ldebug.c",
            "ldo.c",     "ldump.c",   "lfunc.c",   "lgc.c",
            "llex.c",    "lmem.c",    "lobject.c", "lopcodes.c",
            "lparser.c", "lstate.c",  "lstring.c", "ltable.c",
            "ltm.c",     "lundump.c", "lvm.c",     "lzio.c",
            // standard library
            "lauxlib.c", "lbaselib.c", "lcorolib.c", "ldblib.c",
            "liolib.c",  "lmathlib.c", "loadlib.c",  "loslib.c",
            "lstrlib.c", "ltablib.c",  "lutf8lib.c", "linit.c",
        },
        .flags = flags.items,
    });

    lua.installHeader(lua_c.path("src/lua.h"), "lua/lua.h");
    lua.installHeader(lua_c.path("src/luaconf.h"), "lua/luaconf.h");
    lua.installHeader(lua_c.path("src/lualib.h"), "lua/lualib.h");
    lua.installHeader(lua_c.path("src/lauxlib.h"), "lua/lauxlib.h");
    lua.installHeader(lua_c.path("src/lua.hpp"), "lua/lua.hpp");

    b.installArtifact(lua);
}
