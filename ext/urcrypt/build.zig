const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "urcrypt",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.linkLibrary(libsecp256k1(b, target, optimize));
    lib.linkLibrary(libargon2(b, target, optimize));
    lib.linkLibrary(libblake3(b, target, optimize));
    lib.linkLibrary(libed25519(b, target, optimize));
    lib.linkLibrary(libge_additions(b, target, optimize));
    lib.linkLibrary(libkeccak_tiny(b, target, optimize));
    lib.linkLibrary(libmonocypher(b, target, optimize));
    lib.linkLibrary(libscrypt(b, target, optimize));

    lib.linkLibrary(libaes_siv(b, target, optimize));
    lib.linkLibrary(openssl.artifact("ssl"));
    lib.linkLibrary(openssl.artifact("crypto"));

    lib.addIncludePath(dep_c.path("urcrypt"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("urcrypt"),
        .files = &.{
            "aes_cbc.c",
            "aes_ecb.c",
            "aes_siv.c",
            "argon.c",
            "blake3.c",
            "chacha.c",
            "ed25519.c",
            "ge_additions.c",
            "keccak.c",
            "ripemd.c",
            "scrypt.c",
            "secp256k1.c",
            "sha.c",
            "util.c",
        },
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
            "-g",
            "-Wall",
        },
    });

    lib.installHeader(dep_c.path("urcrypt/urcrypt.h"), "urcrypt.h");

    b.installArtifact(lib);
}

fn libaes_siv(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const dep_c = b.dependency("aes_siv", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "aes_siv",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibrary(openssl.artifact("ssl"));
    lib.linkLibrary(openssl.artifact("crypto"));

    const config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{});
    lib.addConfigHeader(config_h);
    lib.addIncludePath(dep_c.path(""));
    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            "aes_siv.c",
        },
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("aes_siv.h"), "aes_siv.h");

    lib.linkLibC();

    return lib;
}

fn libsecp256k1(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("secp256k1", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "secp256k1",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("src"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("src"),
        .files = &.{
            "precomputed_ecmult.c",
            "precomputed_ecmult_gen.c",
            "secp256k1.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-g",
            "-O2",
            "-fno-omit-frame-pointer",
            "-std=c89",
            "-pedantic",
            "-Wno-long-long",
            "-Wnested-externs",
            "-Wshadow",
            "-Wstrict-prototypes",
            "-Wundef",
            "-Wno-overlength-strings",
            "-Wall",
            "-Wno-unused-function",
            "-Wextra",
            "-Wcast-align",
            "-Wconditional-uninitialized",
            "-fvisibility=hidden",

            // "-DHAVE_CONFIG_H",
            // "-dexhaustive_test_ORDER=7",
            "-DECMULT_WINDOW_SIZE=15",
            "-DCOMB_BLOCKS=43",
            "-DCOMB_TEETH=6",
            "-DENABLE_MODULE_ELLSWIFT=1",
            "-DENABLE_MODULE_SCHNORRSIG=1",
            "-DENABLE_MODULE_EXTRAKEYS=1",
            "-DENABLE_MODULE_RECOVERY=1",
            "-DENABLE_MODULE_ECDH=1",
        },
    });

    lib.installHeadersDirectory(dep_c.path("include"), "", .{});

    return lib;
}

fn libargon2(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "argon2",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    const flags = .{
        "-O2",
        "-fno-omit-frame-pointer",
        "-fno-sanitize=all",
        "-Wno-unused-value",
        "-Wno-unused-function",
        "-DARGON2_NO_THREADS",
    };

    const common_files = .{
        "src/argon2.c",
        "src/core.c",
        "src/blake2/blake2b.c",
        "src/encoding.c",
        "src/thread.c",
    };

    lib.addIncludePath(dep_c.path("argon2/include"));
    lib.addIncludePath(dep_c.path("argon2/src"));
    lib.addIncludePath(dep_c.path("argon2/src/blake2"));

    if (target.result.cpu.arch == .x86_64) {
        lib.addCSourceFiles(.{
            .root = dep_c.path("argon2"),
            .files = &(common_files ++ .{"src/opt.c"}),
            .flags = &flags,
        });
    } else {
        lib.addCSourceFiles(.{
            .root = dep_c.path("argon2"),
            .files = &(common_files ++ .{"src/ref.c"}),
            .flags = &flags,
        });
    }

    lib.installHeader(dep_c.path("argon2/include/argon2.h"), "argon2.h");
    lib.installHeader(dep_c.path("argon2/src/blake2/blake2.h"), "blake2.h");

    return lib;
}

fn libblake3(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "blake3",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    const common_files = .{
        "blake3.c",
        "blake3_dispatch.c",
        "blake3_portable.c",
    };

    lib.addIncludePath(dep_c.path("blake3"));

    if (target.result.cpu.arch == .x86_64) {
        lib.addCSourceFiles(.{
            .root = dep_c.path("blake3"),
            .files = &(common_files ++ .{
                "blake3_sse2_x86-64_unix.S",
                "blake3_sse41_x86-64_unix.S",
                "blake3_avx2_x86-64_unix.S",
                "blake3_avx512_x86-64_unix.S",
            }),
            .flags = &.{
                "-O2",
                "-fno-omit-frame-pointer",
                "-fno-sanitize=all",
            },
        });
    } else {
        lib.addCSourceFiles(.{
            .root = dep_c.path("blake3"),
            .files = &(common_files ++ .{"blake3_neon.c"}),
            .flags = &.{
                "-O2",
                "-fno-omit-frame-pointer",
                "-fno-sanitize=all",
                "-DBLAKE3_USE_NEON=1",
            },
        });
    }

    lib.installHeader(dep_c.path("blake3/blake3.h"), "blake3.h");
    lib.installHeader(dep_c.path("blake3/blake3_impl.h"), "blake3_impl.h");

    return lib;
}

fn libed25519(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "ed25519",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("ed25519/src"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("ed25519"),
        .files = &.{
            "src/add_scalar.c",
            "src/fe.c",
            "src/ge.c",
            "src/keypair.c",
            "src/key_exchange.c",
            "src/sc.c",
            "src/seed.c",
            "src/sha512.c",
            "src/sign.c",
            "src/verify.c",
        },
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
            "-Wno-unused-result",
        },
    });

    lib.installHeadersDirectory(dep_c.path("ed25519/src"), "", .{
        .include_extensions = &.{".h"},
    });

    return lib;
}

fn libge_additions(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "ge_additions",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("ed25519/src"));
    lib.addIncludePath(dep_c.path("ge-additions"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("ge-additions"),
        .files = &.{"ge-additions.c"},
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
            "-Werror",
            "-pedantic",
            "-std=gnu99",
        },
    });

    lib.installHeader(dep_c.path("ge-additions/ge-additions.h"), "ge-additions.h");

    return lib;
}

fn libkeccak_tiny(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "keccak_tiny",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("keccak-tiny"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("keccak-tiny"),
        .files = &.{"keccak-tiny.c"},
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
            "-std=c11",
            "-Wextra",
            "-Wpedantic",
            "-Wall",
        },
    });

    lib.installHeader(dep_c.path("keccak-tiny/keccak-tiny.h"), "keccak-tiny.h");

    return lib;
}

fn libmonocypher(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "monocypher",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("monocypher"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("monocypher"),
        .files = &.{"monocypher.c"},
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("monocypher/monocypher.h"), "monocypher.h");

    return lib;
}

fn libscrypt(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep_c = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "scrypt",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.addIncludePath(dep_c.path("scrypt"));

    lib.addCSourceFiles(.{
        .root = dep_c.path("scrypt"),
        .files = &.{
            "b64.c",
            "crypto-mcf.c",
            "crypto-scrypt-saltgen.c",
            "crypto_scrypt-check.c",
            "crypto_scrypt-hash.c",
            "crypto_scrypt-hexconvert.c",
            "crypto_scrypt-nosse.c",
            // "main.c",
            "sha256.c",
            "slowequals.c",
        },
        .flags = &.{
            "-O2",
            "-fno-omit-frame-pointer",
            "-fno-sanitize=all",
            "-D_FORTIFY_SOURCE=2",
        },
    });

    lib.installHeader(dep_c.path("scrypt/libscrypt.h"), "libscrypt.h");
    lib.installHeader(dep_c.path("scrypt/sha256.h"), "sha256.h");

    return lib;
}
