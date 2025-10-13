const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const patches = b.dependency("patches", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
    });

    const wslay = b.dependency("wslay", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o_c = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const sse2neon_c = b.dependency("sse2neon", .{
        .target = target,
        .optimize = optimize,
    });

    const cloexec = b.addStaticLibrary(.{
        .name = "cloexec",
        .target = target,
        .optimize = optimize,
    });

    cloexec.linkLibC();

    cloexec.addIncludePath(h2o_c.path("deps/cloexec"));

    cloexec.addCSourceFiles(.{
        .root = h2o_c.path("deps/cloexec"),
        .files = &.{"cloexec.c"},
        .flags = &.{
            "-fno-sanitize=all",
            if (t.isGnuLibC())
                "-D_GNU_SOURCE"
            else
                "",
        },
    });

    cloexec.installHeader(h2o_c.path("deps/cloexec/cloexec.h"), "cloexec.h");

    const klib = b.addStaticLibrary(.{
        .name = "klib",
        .target = target,
        .optimize = optimize,
    });

    klib.linkLibrary(curl.artifact("curl"));
    klib.linkLibrary(zlib.artifact("z"));
    klib.linkLibC();

    klib.addIncludePath(h2o_c.path("deps/klib"));
    if (t.cpu.arch == .aarch64) {
        klib.addIncludePath(sse2neon_c.path("."));
    }

    klib.addCSourceFiles(.{
        .root = h2o_c.path("deps/klib"),
        .files = &.{
            "bgzf.c",
            "khmm.c",
            "kmath.c",
            "knetfile.c",
            "knhx.c",
            // "kopen.c",
            "ksa.c",
            "kson.c",
            "kstring.c",
            // "ksw.c",
            "kthread.c",
            "kurl.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });
    klib.addCSourceFiles(.{
        .root = patches.path("h2o-2.2.6/deps/klib"),
        .files = &.{
            "ksw.c",
            "kopen.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            if (t.cpu.arch == .aarch64)
                "-DURBIT_RUNTIME_CPU_AARCH64"
            else
                "",
        },
    });

    klib.installHeadersDirectory(h2o_c.path("deps/klib"), "", .{
        .include_extensions = &.{".h"},
    });

    const libgkc = b.addStaticLibrary(.{
        .name = "libgkc",
        .target = target,
        .optimize = optimize,
    });

    libgkc.linkLibC();

    libgkc.addIncludePath(h2o_c.path("deps/libgkc"));

    libgkc.addCSourceFiles(.{
        .root = h2o_c.path("deps/libgkc"),
        .files = &.{"gkc.c"},
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    libgkc.installHeader(h2o_c.path("deps/libgkc/gkc.h"), "gkc.h");

    const libyrmcds = b.addStaticLibrary(.{
        .name = "libyrmcds",
        .target = target,
        .optimize = optimize,
    });

    libyrmcds.linkLibC();

    libyrmcds.addIncludePath(h2o_c.path("deps/libyrmcds"));

    libyrmcds.addCSourceFiles(.{
        .root = h2o_c.path("deps/libyrmcds"),
        .files = &.{
            "close.c",
            "connect.c",
            "counter.c",
            "recv.c",
            "send.c",
            "send_text.c",
            "set_compression.c",
            "socket.c",
            "strerror.c",
            "text_mode.c",
            // "yc-cnt.c",
            // "yc.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-Wall",
            "-Wconversion",
            "-gdwarf-3",
            "-O2",
        },
    });

    libyrmcds.installHeadersDirectory(h2o_c.path("deps/libyrmcds"), "", .{
        .include_extensions = &.{".h"},
    });

    const picohttpparser = b.addStaticLibrary(.{
        .name = "picohttpparser",
        .target = target,
        .optimize = optimize,
    });

    picohttpparser.linkLibC();

    picohttpparser.addIncludePath(h2o_c.path("deps/picohttpparser"));
    if (t.cpu.arch == .aarch64) {
        picohttpparser.addIncludePath(sse2neon_c.path("."));
    }

    picohttpparser.addCSourceFiles(.{
        .root = patches.path("h2o-2.2.6/deps/picohttpparser"),
        .files = &.{"picohttpparser.c"},
        .flags = &.{
            if (t.cpu.arch == .aarch64)
                "-DURBIT_RUNTIME_CPU_AARCH64"
            else
                "",
        },
    });

    picohttpparser.installHeadersDirectory(h2o_c.path("deps/picohttpparser"), "", .{
        .include_extensions = &.{".h"},
    });

    const cifra = b.addStaticLibrary(.{
        .name = "cifra",
        .target = target,
        .optimize = optimize,
    });

    cifra.linkLibC();

    cifra.addIncludePath(h2o_c.path("deps/picotls/deps/cifra/src"));
    cifra.addIncludePath(h2o_c.path("deps/picotls/deps/cifra/src/ext"));

    cifra.addCSourceFiles(.{
        .root = h2o_c.path("deps/picotls/deps/cifra/src"),
        .files = &.{
            "aes.c",
            "sha256.c",
            "sha512.c",
            "chash.c",
            "hmac.c",
            "pbkdf2.c",
            "modes.c",
            "eax.c",
            "gf128.c",
            "blockwise.c",
            "cmac.c",
            "salsa20.c",
            "chacha20.c",
            "curve25519.c",
            "gcm.c",
            "cbcmac.c",
            "ccm.c",
            "sha3.c",
            "sha1.c",
            "poly1305.c",
            "norx.c",
            "chacha20poly1305.c",
            "drbg.c",
            "ocb.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    cifra.installHeadersDirectory(h2o_c.path("deps/picotls/deps/cifra/src"), "", .{
        .include_extensions = &.{ ".h", "curve25519.tweetnacl.c" },
    });

    const micro_ecc = b.addStaticLibrary(.{
        .name = "micro_ecc",
        .target = target,
        .optimize = optimize,
    });

    micro_ecc.linkLibC();

    micro_ecc.addIncludePath(h2o_c.path("deps/picotls/deps/micro-ecc"));

    micro_ecc.addCSourceFiles(.{
        .root = h2o_c.path("deps/picotls/deps/micro-ecc"),
        .files = &.{"uECC.c"},
    });

    micro_ecc.installHeadersDirectory(h2o_c.path("deps/picotls/deps/micro-ecc"), "", .{
        .include_extensions = &.{ ".h", ".inc" },
    });

    const picotls = b.addStaticLibrary(.{
        .name = "picotls",
        .target = target,
        .optimize = optimize,
    });

    picotls.linkLibrary(openssl.artifact("ssl"));
    picotls.linkLibrary(cifra);
    picotls.linkLibrary(micro_ecc);
    picotls.linkLibC();

    picotls.addIncludePath(h2o_c.path("deps/picotls/include"));
    if (t.cpu.arch == .aarch64) {
        picotls.addIncludePath(sse2neon_c.path("."));
    }

    picotls.addCSourceFiles(.{
        .root = h2o_c.path("deps/picotls/lib"),
        .files = &.{
            "asn1.c",
            "cifra.c",
            "minicrypto-pem.c",
            "openssl.c",
            "pembase64.c",
            "picotls.c",
            "uecc.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-std=c99",
            "-Wall",
            "-O2",
        },
    });

    picotls.installHeadersDirectory(h2o_c.path("deps/picotls/include"), "", .{
        .include_extensions = &.{".h"},
    });

    // const ssl_conservatory = b.addStaticLibrary(.{
    //     .name = "ssl_conservatory",
    //     .target = target,
    //     .optimize = optimize,
    // });

    // ssl_conservatory.linkLibrary(openssl.artifact("ssl"));
    // ssl_conservatory.linkLibC();

    // ssl_conservatory.addIncludePath(h2o_c.path("deps/ssl-conservatory/openssl"));

    // ssl_conservatory.addCSourceFiles(.{
    //     .root = h2o_c.path("deps/ssl-conservatory/openssl"),
    //     .files = &.{"openssl_hostname_validation.c"},
    // });

    // ssl_conservatory.installHeader(h2o_c.path("deps/ssl-conservatory/openssl/openssl_hostname_validation.h"), "openssl_hostname_validation.h");

    const h2o = b.addStaticLibrary(.{
        .name = "h2o",
        .target = target,
        .optimize = optimize,
    });

    h2o.linkLibrary(openssl.artifact("ssl"));
    h2o.linkLibrary(openssl.artifact("crypto"));
    h2o.linkLibrary(zlib.artifact("z"));
    h2o.linkLibrary(libuv.artifact("libuv"));
    h2o.linkLibrary(cloexec);
    h2o.linkLibrary(klib);
    h2o.linkLibrary(libgkc);
    h2o.linkLibrary(libyrmcds);
    h2o.linkLibrary(picohttpparser);
    h2o.linkLibrary(picotls);
    h2o.linkLibrary(wslay.artifact("wslay"));
    // h2o.linkLibrary(ssl_conservatory);
    h2o.linkLibC();

    h2o.addIncludePath(h2o_c.path("deps/golombset"));
    h2o.addIncludePath(h2o_c.path("deps/yoml"));

    h2o.addIncludePath(h2o_c.path("include"));
    h2o.addIncludePath(h2o_c.path("include/h2o"));
    h2o.addIncludePath(h2o_c.path("include/h2o/socket"));
    h2o.addIncludePath(wslay.path("lib/includes"));

    h2o.addCSourceFiles(.{
        .root = h2o_c.path("lib"),
        .files = &.{
            "common/cache.c",
            "common/file.c",
            "common/filecache.c",
            "common/hostinfo.c",
            "common/http1client.c",
            "common/memcached.c",
            "common/memory.c",
            "common/multithread.c",
            "common/serverutil.c",
            "common/socket.c",
            "common/socketpool.c",
            "common/string.c",
            "common/time.c",
            "common/timeout.c",
            "common/url.c",
            "core/config.c",
            "core/configurator.c",
            "core/context.c",
            "core/headers.c",
            "core/logconf.c",
            "core/proxy.c",
            "core/request.c",
            "core/token.c",
            "core/util.c",
            "handler/access_log.c",
            "handler/chunked.c",
            "handler/compress.c",
            "handler/compress/gzip.c",
            "handler/configurator/access_log.c",
            "handler/configurator/compress.c",
            "handler/configurator/errordoc.c",
            "handler/configurator/expires.c",
            "handler/configurator/fastcgi.c",
            "handler/configurator/file.c",
            "handler/configurator/headers.c",
            "handler/configurator/headers_util.c",
            "handler/configurator/http2_debug_state.c",
            "handler/configurator/proxy.c",
            "handler/configurator/redirect.c",
            "handler/configurator/reproxy.c",
            "handler/configurator/status.c",
            "handler/configurator/throttle_resp.c",
            "handler/errordoc.c",
            "handler/expires.c",
            "handler/fastcgi.c",
            "handler/file.c",
            "handler/headers.c",
            "handler/headers_util.c",
            "handler/http2_debug_state.c",
            "handler/mimemap.c",
            "handler/proxy.c",
            "handler/redirect.c",
            "handler/reproxy.c",
            "handler/status.c",
            "handler/status/durations.c",
            "handler/status/events.c",
            "handler/status/requests.c",
            "handler/throttle_resp.c",
            "http1.c",
            "http2/cache_digests.c",
            "http2/casper.c",
            "http2/connection.c",
            "http2/frame.c",
            "http2/hpack.c",
            "http2/http2_debug_state.c",
            "http2/scheduler.c",
            "http2/stream.c",
            "websocket.c",
            "tunnel.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-std=c99",
            "-g3",
            "-O2",
            "-pthread",
            "-DH2O_USE_LIBUV",
            "-DH2O_USE_PICOTLS",
            "-DWSLAY_VERSION=\\\"1.1.1\\\"",
            if (t.os.tag == .linux) "-D_GNU_SOURCE" else "",
        },
    });

    h2o.installHeadersDirectory(h2o_c.path("include"), "", .{});

    b.installArtifact(h2o);
}
