const std = @import("std");

const VERSION = "3.2";

const targets: []const std.Target.Query = &.{
    .{ .cpu_arch = .aarch64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .x86_64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .musl },
};

const BuildCfg = struct {
    version: []const u8,
    pace: []const u8,
    binary_name: []const u8,
    flags: []const []const u8 = &.{},
    include_test_steps: bool = true,
    cpu_dbg: bool = false,
    mem_dbg: bool = false,
    c3dbg: bool = false,
    snapshot_validation: bool = false,
};

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{ .whitelist = targets });
    var optimize = b.standardOptimizeOption(.{});

    //
    // Additional Project-Specific Options
    //

    const all = b.option(bool, "all", "Build all targets") orelse false;

    const release = b.option(bool, "release", "Build for release") orelse false;
    if (release) optimize = .ReleaseFast;

    const Pace = enum { once, live, soon, edge };
    const pace = @tagName(b.option(
        Pace,
        "pace",
        "Release train (Default: once)",
    ) orelse if (release) Pace.live else Pace.once);

    const flags_opt = b.option(
        []const u8,
        "flags",
        "Provide additional compiler flags",
    );
    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    var iter_flags = std.mem.splitSequence(u8, flags_opt orelse "", " ");
    while (iter_flags.next()) |flag| {
        if (flag.len != 0) {
            try flags.appendSlice(&.{flag});
        }
    }

    const cpu_dbg = b.option(
        bool,
        "cpu-dbg",
        "Enable cpu debug mode (-DU3_CPU_DEBUG)",
    ) orelse false;

    const mem_dbg = b.option(
        bool,
        "mem-dbg",
        "Enable memory debug mode (-DU3_MEMORY_DEBUG)",
    ) orelse false;

    const c3dbg = b.option(
        bool,
        "c3dbg",
        "Enable debug assertions (-DC3DBG)",
    ) orelse false;

    const snapshot_validation = b.option(
        bool,
        "snapshot-validation",
        "Enable snapshot validation (-DU3_SNAPSHOT_VALIDATION)",
    ) orelse false;

    const binary_name = b.option(
        []const u8,
        "binary-name",
        "Binary name (Default: urbit)",
    ) orelse "urbit";

    // Parse short git rev
    //
    var file = try std.fs.cwd().openFile(".git/logs/HEAD", .{});
    defer file.close();
    var buf_reader = std.io.bufferedReader(file.reader());
    var in_stream = buf_reader.reader();
    var buf: [1024]u8 = undefined;
    var last_line: [1024]u8 = undefined;
    while (try in_stream.readUntilDelimiterOrEof(&buf, '\n')) |line| {
        if (line.len > 0)
            last_line = buf;
    }
    const git_rev = buf[41..51];

    // Binary version
    const version = if (!release)
        VERSION ++ "-" ++ git_rev
    else
        VERSION;

    //
    // BUILD
    //

    const build_cfg: BuildCfg = .{
        .version = version,
        .pace = pace,
        .flags = flags.items,
        .binary_name = binary_name,
        .cpu_dbg = cpu_dbg,
        .mem_dbg = mem_dbg,
        .c3dbg = c3dbg,
        .snapshot_validation = snapshot_validation,
        .include_test_steps = !all,
    };

    if (all) {
        for (targets) |t| {
            try build_single(b, b.resolveTargetQuery(t), optimize, build_cfg);
        }
    } else {
        const t = target.result;
        try build_single(
            b,
            if (t.os.tag == .linux and target.query.isNative())
                b.resolveTargetQuery(.{ .abi = .musl })
            else
                target,
            optimize,
            build_cfg,
        );
    }
}

fn build_single(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cfg: BuildCfg,
) !void {
    const t = target.result;

    //
    // CFLAGS for both dependencies and build
    //

    var global_flags = std.ArrayList([]const u8).init(b.allocator);
    defer global_flags.deinit();

    try global_flags.appendSlice(cfg.flags);
    try global_flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-g",
        "-Wall",
        "-Werror",
    });

    //
    // CFLAGS for Urbit Libs and Binaries
    //

    var urbit_flags = std.ArrayList([]const u8).init(b.allocator);
    defer urbit_flags.deinit();

    try urbit_flags.appendSlice(global_flags.items);
    try urbit_flags.appendSlice(&.{
        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fms-extensions",
        "-DU3_GUARD_PAGE", // pkg_noun
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

    if (cfg.cpu_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_CPU_DEBUG"});

    if (cfg.mem_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_MEMORY_DEBUG"});

    if (cfg.c3dbg)
        try urbit_flags.appendSlice(&.{"-DC3DBG"});

    if (cfg.snapshot_validation)
        try urbit_flags.appendSlice(&.{"-DU3_SNAPSHOT_VALIDATION"});

    if (t.cpu.arch == .aarch64) {
        try urbit_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .macos) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
            "-DENT_GETENTROPY_SYSRANDOM", // pkg_ent
        });
    } else {
        try urbit_flags.appendSlice(&.{
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

    if (t.os.tag == .linux) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
        });
    }

    //
    // DEPENDENCIES
    //

    const avahi = b.dependency("avahi", .{
        .target = target,
        .optimize = optimize,
    });

    const backtrace = b.dependency("backtrace", .{
        .target = target,
        .optimize = optimize,
    });

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const murmur3 = b.dependency("murmur3", .{
        .target = target,
        .optimize = optimize,
    });

    const natpmp = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const pdjson = b.dependency("pdjson", .{
        .target = target,
        .optimize = optimize,
    });

    const sigsegv = b.dependency("sigsegv", .{
        .target = target,
        .optimize = optimize,
    });

    const softblas = b.dependency("softblas", .{
        .target = target,
        .optimize = optimize,
    });

    const softfloat = b.dependency("softfloat", .{
        .target = target,
        .optimize = optimize,
    });

    const unwind = b.dependency("unwind", .{
        .target = target,
        .optimize = optimize,
    });

    const urcrypt = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const whereami = b.dependency("whereami", .{
        .target = target,
        .optimize = optimize,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
    });

    //
    // Install artifacts
    //

    const pkg_c3 = b.addStaticLibrary(.{
        .name = "c3",
        .target = target,
        .optimize = optimize,
    });

    const pkg_ent = b.addStaticLibrary(.{
        .name = "ent",
        .target = target,
        .optimize = optimize,
    });

    const pkg_ur = b.addStaticLibrary(.{
        .name = "ur",
        .target = target,
        .optimize = optimize,
    });

    const pkg_noun = b.addStaticLibrary(.{
        .name = "noun",
        .target = target,
        .optimize = optimize,
    });

    const vere = b.addStaticLibrary(.{
        .name = "vere",
        .target = target,
        .optimize = optimize,
    });

    const urbit = b.addExecutable(.{
        .name = cfg.binary_name,
        .target = target,
        .optimize = optimize,
    });

    const artifacts = [_]*std.Build.Step.Compile{
        pkg_c3,
        pkg_ent,
        pkg_ur,
        pkg_noun,
        vere,
        urbit,
    };

    for (artifacts) |artifact| {
        const target_query: std.Target.Query = .{
            .cpu_arch = t.cpu.arch,
            .os_tag = t.os.tag,
            .abi = t.abi,
            .os_version_min = .none,
            .os_version_max = .none,
        };
        const target_output = b.addInstallArtifact(artifact, .{
            .dest_dir = .{
                .override = .{
                    .custom = try target_query.zigTriple(b.allocator),
                },
            },
        });
        b.getInstallStep().dependOn(&target_output.step);
    }

    if (target.result.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });

        const steps = [_]*std.Build.Step.Compile{
            pkg_c3,
            pkg_noun,
            vere,
            urbit,
        };

        for (steps) |step| {
            if (macos_sdk != null) {
                step.addSystemIncludePath(macos_sdk.?.path("usr/include"));
                step.addLibraryPath(macos_sdk.?.path("usr/lib"));
                step.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
            }
        }
    }

    //
    // PKG C3
    //

    pkg_c3.linkLibC();

    pkg_c3.addIncludePath(b.path("pkg/c3"));

    pkg_c3.addCSourceFiles(.{
        .root = b.path("pkg/c3"),
        .files = &.{"defs.c"},
        .flags = urbit_flags.items,
    });

    pkg_c3.installHeadersDirectory(b.path("pkg/c3"), "c3", .{
        .include_extensions = &.{".h"},
    });

    // b.installArtifact(pkg_c3);

    //
    // PKG ENT
    //

    pkg_ent.linkLibC();

    pkg_ent.addIncludePath(b.path("pkg/ent"));

    var ent_flags = std.ArrayList([]const u8).init(b.allocator);
    defer ent_flags.deinit();

    try ent_flags.appendSlice(&.{
        "-pedantic",
        "-std=gnu99",
    });
    try ent_flags.appendSlice(urbit_flags.items);

    pkg_ent.addCSourceFiles(.{
        .root = b.path("pkg/ent"),
        .files = &.{"ent.c"},
        .flags = ent_flags.items,
    });

    pkg_ent.installHeadersDirectory(b.path("pkg/ent"), "ent", .{
        .include_extensions = &.{".h"},
    });

    // b.installArtifact(pkg_ent);

    //
    // PKG UR
    //

    pkg_ur.linkLibrary(murmur3.artifact("murmur3"));
    pkg_ur.linkLibC();

    pkg_ur.addIncludePath(b.path("pkg/ur"));

    pkg_ur.addCSourceFiles(.{
        .root = b.path("pkg/ur"),
        .files = &.{
            "bitstream.c",
            "hashcons.c",
            "serial.c",
        },
        .flags = urbit_flags.items,
    });

    pkg_ur.installHeadersDirectory(b.path("pkg/ur"), "ur", .{
        .include_extensions = &.{".h"},
    });

    // b.installArtifact(pkg_ur);

    //
    // PKG NOUN
    //

    pkg_noun.linkLibrary(pkg_c3);
    pkg_noun.linkLibrary(pkg_ent);
    pkg_noun.linkLibrary(pkg_ur);
    pkg_noun.linkLibrary(backtrace.artifact("backtrace"));
    pkg_noun.linkLibrary(gmp.artifact("gmp"));
    pkg_noun.linkLibrary(murmur3.artifact("murmur3"));
    pkg_noun.linkLibrary(openssl.artifact("ssl"));
    pkg_noun.linkLibrary(pdjson.artifact("pdjson"));
    pkg_noun.linkLibrary(sigsegv.artifact("sigsegv"));
    pkg_noun.linkLibrary(softfloat.artifact("softfloat"));
    pkg_noun.linkLibrary(softblas.artifact("softblas"));
    if (t.os.tag == .linux)
        pkg_noun.linkLibrary(unwind.artifact("unwind"));
    pkg_noun.linkLibrary(urcrypt.artifact("urcrypt"));
    pkg_noun.linkLibrary(whereami.artifact("whereami"));
    pkg_noun.linkLibrary(zlib.artifact("z"));
    pkg_noun.linkLibC();

    pkg_noun.addIncludePath(b.path("pkg/noun"));
    if (t.os.tag.isDarwin())
        pkg_noun.addIncludePath(b.path("pkg/noun/platform/darwin"));
    if (t.os.tag == .linux)
        pkg_noun.addIncludePath(b.path("pkg/noun/platform/linux"));

    var noun_flags = std.ArrayList([]const u8).init(b.allocator);
    defer noun_flags.deinit();

    try noun_flags.appendSlice(&.{
        "-pedantic",
        "-std=gnu23",
    });
    try noun_flags.appendSlice(urbit_flags.items);

    pkg_noun.addCSourceFiles(.{
        .root = b.path("pkg/noun"),
        .files = &.{
            "allocate.c",
            "events.c",
            "hashtable.c",
            "imprison.c",
            "jets.c",
            "jets/a/add.c",
            "jets/a/dec.c",
            "jets/a/div.c",
            "jets/a/gte.c",
            "jets/a/gth.c",
            "jets/a/lte.c",
            "jets/a/lth.c",
            "jets/a/max.c",
            "jets/a/min.c",
            "jets/a/mod.c",
            "jets/a/mul.c",
            "jets/a/sub.c",
            "jets/b/bind.c",
            "jets/b/clap.c",
            "jets/b/drop.c",
            "jets/b/find.c",
            "jets/b/flop.c",
            "jets/b/lent.c",
            "jets/b/levy.c",
            "jets/b/lien.c",
            "jets/b/mate.c",
            "jets/b/murn.c",
            "jets/b/need.c",
            "jets/b/reap.c",
            "jets/b/reel.c",
            "jets/b/roll.c",
            "jets/b/scag.c",
            "jets/b/skid.c",
            "jets/b/skim.c",
            "jets/b/skip.c",
            "jets/b/slag.c",
            "jets/b/snag.c",
            "jets/b/sort.c",
            "jets/b/turn.c",
            "jets/b/weld.c",
            "jets/b/zing.c",
            "jets/c/aor.c",
            "jets/c/bex.c",
            "jets/c/c0n.c",
            "jets/c/can.c",
            "jets/c/cap.c",
            "jets/c/cat.c",
            "jets/c/clz.c",
            "jets/c/ctz.c",
            "jets/c/cut.c",
            "jets/c/dis.c",
            "jets/c/dor.c",
            "jets/c/dvr.c",
            "jets/c/end.c",
            "jets/c/gor.c",
            "jets/c/ham.c",
            "jets/c/hew.c",
            "jets/c/lsh.c",
            "jets/c/mas.c",
            "jets/c/met.c",
            "jets/c/mix.c",
            "jets/c/mor.c",
            "jets/c/mug.c",
            "jets/c/muk.c",
            "jets/c/peg.c",
            "jets/c/po.c",
            "jets/c/pow.c",
            "jets/c/rap.c",
            "jets/c/rep.c",
            "jets/c/rev.c",
            "jets/c/rig.c",
            "jets/c/rip.c",
            "jets/c/rsh.c",
            "jets/c/sqt.c",
            "jets/c/swp.c",
            "jets/c/xeb.c",
            "jets/d/by_all.c",
            "jets/d/by_any.c",
            "jets/d/by_apt.c",
            "jets/d/by_bif.c",
            "jets/d/by_del.c",
            "jets/d/by_dif.c",
            "jets/d/by_gas.c",
            "jets/d/by_get.c",
            "jets/d/by_has.c",
            "jets/d/by_int.c",
            "jets/d/by_jab.c",
            "jets/d/by_key.c",
            "jets/d/by_put.c",
            "jets/d/by_run.c",
            "jets/d/by_uni.c",
            "jets/d/by_urn.c",
            "jets/d/in_apt.c",
            "jets/d/in_bif.c",
            "jets/d/in_del.c",
            "jets/d/in_dif.c",
            "jets/d/in_gas.c",
            "jets/d/in_has.c",
            "jets/d/in_int.c",
            "jets/d/in_put.c",
            "jets/d/in_rep.c",
            "jets/d/in_run.c",
            "jets/d/in_tap.c",
            "jets/d/in_uni.c",
            "jets/d/in_wyt.c",
            "jets/e/aes_cbc.c",
            "jets/e/aes_ecb.c",
            "jets/e/aes_siv.c",
            "jets/e/argon2.c",
            "jets/e/base.c",
            "jets/e/blake.c",
            "jets/e/chacha.c",
            "jets/e/crc32.c",
            "jets/e/cue.c",
            "jets/e/ed_add_double_scalarmult.c",
            "jets/e/ed_add_scalarmult_scalarmult_base.c",
            "jets/e/ed_point_add.c",
            "jets/e/ed_puck.c",
            "jets/e/ed_scalarmult.c",
            "jets/e/ed_scalarmult_base.c",
            "jets/e/ed_shar.c",
            "jets/e/ed_sign.c",
            "jets/e/ed_veri.c",
            "jets/e/fein_ob.c",
            "jets/e/fl.c",
            "jets/e/fynd_ob.c",
            "jets/e/hmac.c",
            "jets/e/jam.c",
            "jets/e/json_de.c",
            "jets/e/json_en.c",
            "jets/e/keccak.c",
            "jets/e/leer.c",
            "jets/e/loss.c",
            "jets/e/lune.c",
            "jets/e/mat.c",
            "jets/e/mink.c",
            "jets/e/mole.c",
            "jets/e/mule.c",
            "jets/e/parse.c",
            "jets/e/rd.c",
            "jets/e/rh.c",
            "jets/e/ripe.c",
            "jets/e/rq.c",
            "jets/e/rs.c",
            "jets/e/rub.c",
            "jets/e/scot.c",
            "jets/e/scow.c",
            "jets/e/scr.c",
            "jets/e/secp.c",
            "jets/e/sha1.c",
            "jets/e/shax.c",
            "jets/e/slaw.c",
            "jets/e/tape.c",
            "jets/e/trip.c",
            "jets/f/cell.c",
            "jets/f/comb.c",
            "jets/f/cons.c",
            "jets/f/core.c",
            "jets/f/face.c",
            "jets/f/fine.c",
            "jets/f/fitz.c",
            "jets/f/flan.c",
            "jets/f/flip.c",
            "jets/f/flor.c",
            "jets/f/fork.c",
            "jets/f/help.c",
            "jets/f/hint.c",
            "jets/f/look.c",
            "jets/f/loot.c",
            "jets/f/ut_crop.c",
            "jets/f/ut_fish.c",
            "jets/f/ut_fuse.c",
            "jets/f/ut_mint.c",
            "jets/f/ut_mull.c",
            "jets/f/ut_nest.c",
            "jets/f/ut_rest.c",
            "jets/g/plot.c",
            "jets/i/lagoon.c",
            "jets/tree.c",
            "jets/137/tree.c",
            "log.c",
            "manage.c",
            "nock.c",
            "options.c",
            "retrieve.c",
            "ship.c",
            "serial.c",
            "trace.c",
            "urth.c",
            "v1/allocate.c",
            "v1/hashtable.c",
            "v1/jets.c",
            "v1/manage.c",
            "v1/nock.c",
            "v1/vortex.c",
            "v2/allocate.c",
            "v2/hashtable.c",
            "v2/jets.c",
            "v2/manage.c",
            "v2/nock.c",
            "v2/vortex.c",
            "v3/hashtable.c",
            "v3/manage.c",
            "v4/manage.c",
            "vortex.c",
            "xtract.c",
            "zave.c",
        },
        .flags = noun_flags.items,
    });

    pkg_noun.installHeadersDirectory(b.path("pkg/noun"), "", .{
        .include_extensions = &.{".h"},
    });

    pkg_noun.installHeader(b.path(switch (t.os.tag) {
        .macos => "pkg/noun/platform/darwin/rsignal.h",
        .linux => "pkg/noun/platform/linux/rsignal.h",
        else => "",
    }), "platform/rsignal.h");

    // b.installArtifact(pkg_noun);

    //
    // VERE LIBRARY
    //

    const pace_h = b.addWriteFile("pace.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(b.fmt(
            \\#ifndef URBIT_PACE_H
            \\#define URBIT_PACE_H
            \\#define U3_VERE_PACE "{s}"
            \\#endif
            \\
        , .{cfg.pace}));

        break :blk try output.toOwnedSlice();
    });

    const version_h = b.addWriteFile("version.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(b.fmt(
            \\#ifndef URBIT_VERSION_H
            \\#define URBIT_VERSION_H
            \\#define URBIT_VERSION "{s}"
            \\#endif
            \\
        , .{cfg.version}));

        break :blk try output.toOwnedSlice();
    });

    vere.addIncludePath(pace_h.getDirectory());
    vere.addIncludePath(version_h.getDirectory());
    vere.addIncludePath(b.path("pkg/vere"));
    vere.addIncludePath(b.path("pkg/vere/ivory"));
    vere.addIncludePath(b.path("pkg/vere/ca_bundle"));

    if (t.os.tag == .linux) {
        vere.linkLibrary(avahi.artifact("dns-sd"));
    }
    vere.linkLibrary(natpmp.artifact("natpmp"));
    vere.linkLibrary(curl.artifact("curl"));
    vere.linkLibrary(gmp.artifact("gmp"));
    vere.linkLibrary(h2o.artifact("h2o"));
    vere.linkLibrary(libuv.artifact("libuv"));
    vere.linkLibrary(lmdb.artifact("lmdb"));
    vere.linkLibrary(openssl.artifact("ssl"));
    vere.linkLibrary(urcrypt.artifact("urcrypt"));
    vere.linkLibrary(zlib.artifact("z"));
    vere.linkLibrary(pkg_c3);
    vere.linkLibrary(pkg_ent);
    vere.linkLibrary(pkg_noun);
    vere.linkLibrary(pkg_ur);
    vere.linkLibC();

    var vere_srcs = std.ArrayList([]const u8).init(b.allocator);
    defer vere_srcs.deinit();

    try vere_srcs.appendSlice(&.{
        "auto.c",
        "ca_bundle/ca_bundle.c",
        "dawn.c",
        "db/lmdb.c",
        "disk.c",
        "foil.c",
        "io/ames.c",
        "io/ames/stun.c",
        "io/behn.c",
        "io/conn.c",
        "io/cttp.c",
        "io/fore.c",
        "io/hind.c",
        "io/http.c",
        "io/lick.c",
        "io/lss.c",
        "io/mesa.c",
        "io/mesa/bitset.c",
        "io/mesa/pact.c",
        "io/term.c",
        "io/unix.c",
        "ivory/ivory.c",
        "king.c",
        "lord.c",
        "mars.c",
        "mdns.c",
        "newt.c",
        "pier.c",
        "save.c",
        "serf.c",
        "time.c",
        "ward.c",
    });

    if (t.os.tag == .macos) {
        try vere_srcs.appendSlice(&.{
            "platform/darwin/daemon.c",
            "platform/darwin/ptty.c",
            "platform/darwin/mach.c",
        });
    }

    if (t.os.tag == .linux) {
        try vere_srcs.appendSlice(&.{
            "platform/linux/daemon.c",
            "platform/linux/ptty.c",
        });
    }

    var vere_flags = std.ArrayList([]const u8).init(b.allocator);
    defer vere_flags.deinit();

    try vere_flags.appendSlice(&.{
        "-std=gnu23",
    });
    try vere_flags.appendSlice(urbit_flags.items);

    vere.addCSourceFiles(.{
        .root = b.path("pkg/vere"),
        .files = vere_srcs.items,
        .flags = vere_flags.items,
    });

    vere.installHeadersDirectory(b.path("pkg/vere"), "", .{
        .include_extensions = &.{".h"},
        .exclude_extensions = &.{ "ivory.h", "ca_bundle.h" },
    });

    vere.installHeader(b.path("pkg/vere/ivory/ivory.h"), "ivory.h");
    vere.installHeader(b.path("pkg/vere/ca_bundle/ca_bundle.h"), "ca_bundle.h");
    vere.installHeader(pace_h.getDirectory().path(b, "pace.h"), "pace.h");
    vere.installHeader(version_h.getDirectory().path(b, "version.h"), "version.h");

    // b.installArtifact(vere);

    //
    // URBIT BINARY
    //

    urbit.stack_size = 0;

    urbit.linkLibC();

    urbit.linkLibrary(vere);
    urbit.linkLibrary(pkg_noun);
    urbit.linkLibrary(pkg_c3);
    urbit.linkLibrary(pkg_ur);

    urbit.linkLibrary(gmp.artifact("gmp"));
    urbit.linkLibrary(h2o.artifact("h2o"));
    urbit.linkLibrary(curl.artifact("curl"));
    urbit.linkLibrary(libuv.artifact("libuv"));
    urbit.linkLibrary(lmdb.artifact("lmdb"));
    urbit.linkLibrary(openssl.artifact("ssl"));
    urbit.linkLibrary(sigsegv.artifact("sigsegv"));
    urbit.linkLibrary(urcrypt.artifact("urcrypt"));
    urbit.linkLibrary(whereami.artifact("whereami"));

    urbit.addCSourceFiles(.{
        .root = b.path("pkg/vere"),
        .files = &.{
            "main.c",
        },
        .flags = urbit_flags.items,
    });

    // b.installArtifact(urbit);

    //
    // Tests
    //

    if (cfg.include_test_steps) {
        // pkg_ur
        add_test(
            b,
            target,
            optimize,
            "ur-test",
            "pkg/ur/tests.c",
            &.{pkg_ur},
            urbit_flags.items,
        );

        // pkg_ent
        add_test(
            b,
            target,
            optimize,
            "ent-test",
            "pkg/ent/tests.c",
            &.{pkg_ent},
            urbit_flags.items,
        );

        // pkg_noun
        add_test(
            b,
            target,
            optimize,
            "hashtable-test",
            "pkg/noun/hashtable_tests.c",
            &.{ pkg_noun, pkg_c3, gmp.artifact("gmp") },
            noun_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "jets-test",
            "pkg/noun/jets_tests.c",
            &.{ pkg_noun, pkg_c3, gmp.artifact("gmp") },
            noun_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "nock-test",
            "pkg/noun/nock_tests.c",
            &.{ pkg_noun, pkg_c3, gmp.artifact("gmp") },
            noun_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "retrieve-test",
            "pkg/noun/retrieve_tests.c",
            &.{ pkg_noun, pkg_c3, gmp.artifact("gmp") },
            noun_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "serial-test",
            "pkg/noun/serial_tests.c",
            &.{ pkg_noun, pkg_c3, gmp.artifact("gmp") },
            noun_flags.items,
        );

        // vere
        add_test(
            b,
            target,
            optimize,
            "ames-test",
            "pkg/vere/ames_tests.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
                zlib.artifact("z"),
            },
            vere_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "boot-test",
            "pkg/vere/boot_tests.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            vere_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "newt-test",
            "pkg/vere/newt_tests.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            vere_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "vere-noun-test",
            "pkg/vere/noun_tests.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            vere_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "unix-test",
            "pkg/vere/unix_tests.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            vere_flags.items,
        );
        add_test(
            b,
            target,
            optimize,
            "benchmarks",
            "pkg/vere/benchmarks.c",
            &.{
                vere,
                pkg_noun,
                pkg_ur,
                pkg_ent,
                pkg_c3,
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            vere_flags.items,
        );
    }
}

fn add_test(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
    file: []const u8,
    deps: []const *std.Build.Step.Compile,
    cflags: []const []const u8,
) void {
    const test_step = b.step(name, b.fmt("Build & run: {s}", .{file}));

    const test_exe = b.addExecutable(.{
        .name = name,
        .target = target,
        .optimize = optimize,
    });

    // const target_output = b.addInstallArtifact(test_exe, .{
    //     .dest_dir = .{
    //         .override = .{
    //             .custom = try target.result.zigTriple(b.allocator),
    //         },
    //     },
    // });
    // b.getInstallStep().dependOn(&target_output.step);

    if (target.result.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            test_exe.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            test_exe.addLibraryPath(macos_sdk.?.path("usr/lib"));
            test_exe.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    test_exe.stack_size = 0;

    test_exe.linkLibC();
    for (deps) |dep| {
        test_exe.linkLibrary(dep);
    }

    test_exe.addCSourceFiles(.{ .files = &.{file}, .flags = cflags });

    const run_unit_tests = b.addRunArtifact(test_exe);
    run_unit_tests.skip_foreign_checks = true;
    test_step.dependOn(&run_unit_tests.step);
}
