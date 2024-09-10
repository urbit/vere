const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    //
    // DEPENDENCIES
    //

    const avahi = b.dependency("avahi", .{
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

    const softfloat = b.dependency("softfloat", .{
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
    // PKG C3
    //

    const pkg_c3 = b.addStaticLibrary(.{
        .name = "c3",
        .target = target,
        .optimize = optimize,
    });

    pkg_c3.linkLibC();

    pkg_c3.addIncludePath(b.path("pkg/c3"));

    var c3_flags = std.ArrayList([]const u8).init(b.allocator);
    defer c3_flags.deinit();

    try c3_flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-DU3_OS_ENDIAN_little=1",
        "-DU3_OS_PROF=1",
    });

    if (t.cpu.arch == .aarch64) {
        try c3_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .linux) {
        try c3_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
        });
    }

    if (t.os.tag == .macos) {
        try c3_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
        });
    }

    pkg_c3.addCSourceFiles(.{
        .root = b.path("pkg/c3"),
        .files = &.{"defs.c"},
        .flags = c3_flags.items,
    });

    pkg_c3.installHeadersDirectory(b.path("pkg/c3"), "c3", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(pkg_c3);

    //
    // PKG ENT
    //

    const pkg_ent = b.addStaticLibrary(.{
        .name = "ent",
        .target = target,
        .optimize = optimize,
    });

    pkg_ent.linkLibC();

    pkg_ent.addIncludePath(b.path("pkg/ent"));

    pkg_ent.addCSourceFiles(.{
        .root = b.path("pkg/ent"),
        .files = &.{"ent.c"},
        .flags = &.{
            "-fno-sanitize=all",
            "-O3",
            "-Wall",
            "-Werror",
            "-pedantic",
            "-std=gnu99",
            // TODO: support fallback to other options if `getrandom()` isn't
            // available in `unistd.h`. Preferred order (from most preferred
            // to least preferred) is:
            // - ENT_GETENTROPY_UNISTD
            // - ENT_GETENTROPY_SYSRANDOM
            // - ENT_GETRANDOM_SYSCALL
            // - ENT_DEV_URANDOM
            if (t.os.tag == .macos)
                "-DENT_GETENTROPY_SYSRANDOM"
            else
                "-DENT_GETENTROPY_UNISTD",
        },
    });

    pkg_ent.installHeadersDirectory(b.path("pkg/ent"), "ent", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(pkg_ent);

    //
    // PKG UR
    //

    const pkg_ur = b.addStaticLibrary(.{
        .name = "ur",
        .target = target,
        .optimize = optimize,
    });

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
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    pkg_ur.installHeadersDirectory(b.path("pkg/ur"), "ur", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(pkg_ur);

    //
    // PKG NOUN
    //

    const pkg_noun = b.addStaticLibrary(.{
        .name = "noun",
        .target = target,
        .optimize = optimize,
    });

    pkg_noun.linkLibrary(pkg_c3);
    pkg_noun.linkLibrary(pkg_ent);
    pkg_noun.linkLibrary(pkg_ur);
    pkg_noun.linkLibrary(gmp.artifact("gmp"));
    pkg_noun.linkLibrary(murmur3.artifact("murmur3"));
    pkg_noun.linkLibrary(openssl.artifact("ssl"));
    pkg_noun.linkLibrary(pdjson.artifact("pdjson"));
    pkg_noun.linkLibrary(sigsegv.artifact("sigsegv"));
    pkg_noun.linkLibrary(softfloat.artifact("softfloat"));
    pkg_noun.linkLibrary(urcrypt.artifact("urcrypt"));
    pkg_noun.linkLibC();

    pkg_noun.addIncludePath(b.path("pkg/noun"));
    pkg_noun.addIncludePath(b.path("pkg/noun/jets"));
    pkg_noun.addIncludePath(b.path(switch (t.os.tag) {
        .macos => "pkg/noun/platform/darwin",
        .linux => "pkg/noun/platform/linux",
        else => "",
    }));
    pkg_noun.addIncludePath(b.path("pkg/noun/v1"));
    pkg_noun.addIncludePath(b.path("pkg/noun/v2"));
    pkg_noun.addIncludePath(b.path("pkg/noun/v3"));

    var noun_flags = std.ArrayList([]const u8).init(b.allocator);
    defer noun_flags.deinit();

    try noun_flags.appendSlice(&.{
        "-fno-sanitize=all",

        "-O3",
        "-g",

        "-Wall",
        "-Werror",
        "-pedantic",
        "-std=gnu23",

        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fms-extensions",

        "-DU3_GUARD_PAGE",
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

    if (t.cpu.arch == .aarch64) {
        try noun_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .macos) {
        try noun_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
            "-DENT_GETENTROPY_SYSRANDOM", // pkg_ent
        });
    }

    if (t.os.tag == .linux) {
        try noun_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

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
            "jets/c/bex.c",
            "jets/c/c0n.c",
            "jets/c/can.c",
            "jets/c/cap.c",
            "jets/c/cat.c",
            "jets/c/cut.c",
            "jets/c/dis.c",
            "jets/c/dor.c",
            "jets/c/dvr.c",
            "jets/c/end.c",
            "jets/c/gor.c",
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
            "jets/tree.c",
            "log.c",
            "manage.c",
            "nock.c",
            "options.c",
            "retrieve.c",
            "serial.c",
            "trace.c",
            "urth.c",
            "allocate_v1.c",
            "hashtable_v1.c",
            "jets_v1.c",
            "manage_v1.c",
            "nock_v1.c",
            "vortex_v1.c",
            "allocate_v2.c",
            "hashtable_v2.c",
            "jets_v2.c",
            "manage_v2.c",
            "nock_v2.c",
            "vortex_v2.c",
            "hashtable_v3.c",
            "manage_v3.c",
            "vortex.c",
            "xtract.c",
            "zave.c",
        },
        .flags = noun_flags.items,
    });

    pkg_noun.installHeadersDirectory(b.path("pkg/noun"), "", .{
        .include_extensions = &.{".h"},
    });

    pkg_noun.installHeadersDirectory(b.path(switch (t.os.tag) {
        .macos => "pkg/noun/platform/darwin",
        .linux => "pkg/noun/platform/linux",
        else => "",
    }), "", .{});

    pkg_noun.installHeadersDirectory(b.path("pkg/noun/jets"), "", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(pkg_noun);

    //
    // VERE LIBRARY
    //

    const vere = b.addStaticLibrary(.{
        .name = "vere",
        .target = target,
        .optimize = optimize,
    });

    const pace_h = b.addWriteFile("pace.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(
            \\#ifndef URBIT_PACE_H
            \\#define URBIT_PACE_H
            \\#define U3_VERE_PACE "once"
            \\#endif
            \\
        );

        break :blk try output.toOwnedSlice();
    });

    const version_h = b.addWriteFile("version.h", blk: {
        var output = std.ArrayList(u8).init(b.allocator);
        defer output.deinit();

        try output.appendSlice(
            \\#ifndef URBIT_VERSION_H
            \\#define URBIT_VERSION_H
            \\#define URBIT_VERSION "3.1-zigidi"
            \\#endif
            \\
        );

        break :blk try output.toOwnedSlice();
    });

    vere.addIncludePath(pace_h.getDirectory());
    vere.addIncludePath(version_h.getDirectory());
    vere.addIncludePath(b.path("pkg/vere"));
    vere.addIncludePath(b.path("pkg/vere/db"));
    vere.addIncludePath(b.path("pkg/vere/io/ames"));
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
        "-fno-sanitize=all",

        "-O3",
        "-g",

        "-Wall",
        "-Werror",
        // "-pedantic",
        "-std=gnu23",

        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fms-extensions",

        "-DU3_GUARD_PAGE",
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

    if (t.cpu.arch == .aarch64) {
        try vere_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .macos) {
        try vere_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
            "-DENT_GETENTROPY_SYSRANDOM", // pkg_ent
        });
    }

    if (t.os.tag == .linux) {
        try vere_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

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

    b.installArtifact(vere);

    //
    // URBIT BINARY
    //

    const urbit = b.addExecutable(.{
        .name = "urbit",
        .target = target,
        .optimize = optimize,
    });

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
    urbit.linkLibrary(whereami.artifact("whereami"));

    var urbit_flags = std.ArrayList([]const u8).init(b.allocator);
    defer urbit_flags.deinit();

    try urbit_flags.appendSlice(&.{
        "-O3",
        "-g",

        "-Wall",
        "-Werror",
        // "-pedantic",
        "-std=gnu23",

        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fms-extensions",

        "-DU3_GUARD_PAGE",
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

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
    }

    if (t.os.tag == .linux) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

    urbit.addCSourceFiles(.{
        .root = b.path("pkg/vere"),
        .files = &.{
            "main.c",
        },
        .flags = urbit_flags.items,
    });

    b.installArtifact(urbit);
}