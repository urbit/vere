const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const aes_siv = b.dependency("aes_siv", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const secp256k1_c = b.dependency("secp256k1", .{
        .target = target,
        .optimize = optimize,
    });

    const secp256k1 = b.addStaticLibrary(.{
        .name = "secp256k1",
        .target = target,
        .optimize = optimize,
    });

    secp256k1.linkLibC();

    secp256k1.addIncludePath(secp256k1_c.path("src"));

    secp256k1.addCSourceFiles(.{
        .root = secp256k1_c.path("src"),
        .files = &.{
            "secp256k1.c",
            "precomputed_ecmult.c",
            "precomputed_ecmult_gen.c",
        },
        .flags = &.{"-O3"},
    });

    secp256k1.installHeadersDirectory(secp256k1_c.path("include"), "", .{});

    const urcrypt_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const urcrypt = b.addStaticLibrary(.{
        .name = "urcrypt",
        .target = target,
        .optimize = optimize,
    });

    urcrypt.linkLibrary(aes_siv.artifact("aes_siv"));
    urcrypt.linkLibrary(openssl.artifact("ssl"));
    urcrypt.linkLibrary(openssl.artifact("crypto"));
    urcrypt.linkLibrary(secp256k1);
    urcrypt.linkLibC();

    urcrypt.addIncludePath(urcrypt_c.path("argon2/include"));
    urcrypt.addIncludePath(urcrypt_c.path("argon2/src"));
    urcrypt.addIncludePath(urcrypt_c.path("argon2/src/blake2"));
    urcrypt.addIncludePath(urcrypt_c.path("blake3"));
    urcrypt.addIncludePath(urcrypt_c.path("ed25519/src"));
    urcrypt.addIncludePath(urcrypt_c.path("ge-additions"));
    urcrypt.addIncludePath(urcrypt_c.path("keccak-tiny"));
    urcrypt.addIncludePath(urcrypt_c.path("scrypt"));
    urcrypt.addIncludePath(urcrypt_c.path("urcrypt"));

    urcrypt.addCSourceFiles(.{
        .root = urcrypt_c.path("urcrypt"),
        .files = &.{
            "aes_cbc.c",
            "aes_ecb.c",
            "aes_siv.c",
            "argon.c",
            "blake3.c",
            "ed25519.c",
            "ge_additions.c",
            "ripemd.c",
            "scrypt.c",
            "keccak.c",
            "secp256k1.c",
            "sha.c",
            "util.c",
        },
        .flags = &.{
            "-Wall",
            "-g",
            "-O3",
        },
    });

    urcrypt.installHeader(urcrypt_c.path("urcrypt/urcrypt.h"), "urcrypt.h");

    b.installArtifact(urcrypt);
}
