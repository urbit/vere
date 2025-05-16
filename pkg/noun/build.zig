const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const copts: []const []const u8 =
        b.option([]const []const u8, "copt", "") orelse &.{};

    const pkg_noun = b.addStaticLibrary(.{
        .name = "noun",
        .target = target,
        .optimize = optimize,
    });

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            pkg_noun.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            pkg_noun.addLibraryPath(macos_sdk.?.path("usr/lib"));
            pkg_noun.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    const pkg_c3 = b.dependency("pkg_c3", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ent = b.dependency("pkg_ent", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ur = b.dependency("pkg_ur", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const backtrace = b.dependency("backtrace", .{
        .target = target,
        .optimize = optimize,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    const murmur3 = b.dependency("murmur3", .{
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

    const wasm3 = b.dependency("wasm3", .{
        .target = target,
        .optimize = optimize,
    });

    pkg_noun.linkLibC();

    pkg_noun.linkLibrary(pkg_c3.artifact("c3"));
    pkg_noun.linkLibrary(pkg_ent.artifact("ent"));
    pkg_noun.linkLibrary(pkg_ur.artifact("ur"));

    pkg_noun.linkLibrary(backtrace.artifact("backtrace"));
    pkg_noun.linkLibrary(gmp.artifact("gmp"));
    pkg_noun.linkLibrary(murmur3.artifact("murmur3"));
    pkg_noun.linkLibrary(openssl.artifact("ssl"));
    pkg_noun.linkLibrary(pdjson.artifact("pdjson"));
    pkg_noun.linkLibrary(sigsegv.artifact("sigsegv"));
    pkg_noun.linkLibrary(softblas.artifact("softblas"));
    pkg_noun.linkLibrary(softfloat.artifact("softfloat"));
    if (t.os.tag == .linux)
        pkg_noun.linkLibrary(unwind.artifact("unwind"));
    pkg_noun.linkLibrary(urcrypt.artifact("urcrypt"));
    pkg_noun.linkLibrary(whereami.artifact("whereami"));
    pkg_noun.linkLibrary(zlib.artifact("z"));
    pkg_noun.linkLibrary(wasm3.artifact("wasm3"));

    pkg_noun.addIncludePath(b.path(""));
    if (t.os.tag.isDarwin())
        pkg_noun.addIncludePath(b.path("platform/darwin"));
    if (t.os.tag == .linux)
        pkg_noun.addIncludePath(b.path("platform/linux"));

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(&.{
        // "-pedantic",
        "-std=gnu23",
    });
    try flags.appendSlice(copts);

    pkg_noun.addCSourceFiles(.{
        .root = b.path(""),
        .files = &c_source_files,
        .flags = flags.items,
    });

    for (install_headers) |h| pkg_noun.installHeader(b.path(h), h);

    pkg_noun.installHeader(b.path(switch (t.os.tag) {
        .macos => "platform/darwin/rsignal.h",
        .linux => "platform/linux/rsignal.h",
        else => "",
    }), "platform/rsignal.h");

    b.installArtifact(pkg_noun);
}

const c_source_files = [_][]const u8{
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
    "jets/c/sew.c",
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
    "jets/e/ed_point_neg.c",
    "jets/e/ed_scad.c",
    "jets/e/ed_recs.c",
    "jets/e/ed_smac.c",
    "jets/e/ed_puck.c",
    "jets/e/ed_luck.c",
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
    "jets/e/urwasm.c",
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
};

const install_headers = [_][]const u8{
    "allocate.h",
    "error.h",
    "events.h",
    "hashtable.h",
    "imprison.h",
    "jets.h",
    "jets/k.h",
    "jets/q.h",
    "jets/w.h",
    "log.h",
    "manage.h",
    "nock.h",
    "noun.h",
    "options.h",
    "retrieve.h",
    "serial.h",
    "ship.h",
    "trace.h",
    "types.h",
    "urth.h",
    "v1/allocate.h",
    "v1/hashtable.h",
    "v1/jets.h",
    "v1/manage.h",
    "v1/nock.h",
    "v1/vortex.h",
    "v2/allocate.h",
    "v2/hashtable.h",
    "v2/jets.h",
    "v2/manage.h",
    "v2/nock.h",
    "v2/options.h",
    "v2/vortex.h",
    "v3/allocate.h",
    "v3/hashtable.h",
    "v3/jets.h",
    "v3/manage.h",
    "v3/nock.h",
    "v3/vortex.h",
    "v4/manage.h",
    "version.h",
    "vortex.h",
    "xtract.h",
    "zave.h",
};
