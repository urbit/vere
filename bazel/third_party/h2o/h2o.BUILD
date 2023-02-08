# This build file is derived from `CMakeLists.txt` in the `h2o` repo at
# https://github.com/h2o/h2o.

# See `CC_WARNING_FLAGS` in `CMakeLists.txt` in the `h2o` repo.
CC_WARNING_FLAGS = [
    "-Wall",
    "-Wno-unused-value",
    "-Wno-unused-function",
    "-Wno-nullability-completeness",
    "-Wno-expansion-to-defined",
    "-Werror=implicit-function-declaration",
    "-Werror=incompatible-pointer-types",
]

#
# H2O DEPENDENCIES
#

# See `deps/cloxec` in the `h2o` repo.
cc_library(
    name = "cloexec",
    srcs = ["deps/cloexec/cloexec.c"],
    hdrs = ["deps/cloexec/cloexec.h"],
    copts = ["-O3"],
    includes = ["deps/cloexec"],
    linkstatic = True,
    visibility = ["//visibility:private"],
)

# See `deps/golombset` in the `h2o` repo.
cc_library(
    name = "golombset",
    hdrs = ["deps/golombset/golombset.h"],
    copts = ["-O3"],
    includes = ["deps/golombset"],
    linkstatic = True,
    visibility = ["//visibility:private"],
)

# See `deps/klib` in the `h2o` repo.
cc_library(
    name = "klib",
    srcs = glob(["deps/klib/*.c"]),
    hdrs = glob(["deps/klib/*.h"]),
    copts = ["-O3"],
    includes = ["deps/klib"],
    linkstatic = True,
    local_defines = select({
        "@platforms//cpu:aarch64": ["URBIT_RUNTIME_CPU_AARCH64"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:private"],
    deps = [
        "@curl",
        "@zlib",
    ] + select({
        "@platforms//cpu:aarch64": ["@sse2neon"],
        "//conditions:default": [],
    }),
)

# See `deps/libgkc` in the `h2o` repo.
cc_library(
    name = "libgkc",
    srcs = ["deps/libgkc/gkc.c"],
    hdrs = ["deps/libgkc/gkc.h"],
    copts = ["-O3"],
    includes = ["deps/libgkc"],
    linkstatic = True,
    visibility = ["//visibility:private"],
)

# See `deps/libyrmcds` in the `h2o` repo.
cc_library(
    name = "libyrmcds",
    srcs = glob(
        ["deps/libyrmcds/*.c"],
        exclude = [
            "deps/libyrmcds/yc.c",
            "deps/libyrmcds/yc-cnt.c",
        ],
    ) + [
        "deps/libyrmcds/yrmcds_portability.h",
        "deps/libyrmcds/yrmcds_text.h",
    ],
    hdrs = ["deps/libyrmcds/yrmcds.h"],
    copts = [
        "-Wall",
        "-Wconversion",
        "-gdwarf-3",
        "-O2",
    ],
    includes = ["deps/libyrmcds"],
    linkstatic = True,
    visibility = ["//visibility:private"],
)

# See `deps/picohttpparser` in the `h2o` repo.
cc_library(
    name = "picohttpparser",
    srcs = ["deps/picohttpparser/picohttpparser.c"],
    hdrs = ["deps/picohttpparser/picohttpparser.h"],
    copts = ["-O3"],
    includes = ["deps/picohttpparser"],
    linkstatic = True,
    local_defines = select({
        "@platforms//cpu:aarch64": ["URBIT_RUNTIME_CPU_AARCH64"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:private"],
    deps = select({
        "@platforms//cpu:aarch64": ["@sse2neon"],
        "//conditions:default": [],
    }),
)

# See `deps/picotls` in the `h2o` repo.
cc_library(
    name = "picotls",
    srcs = glob(
        ["deps/picotls/lib/*.c"],
    ),
    hdrs = ["deps/picotls/include/picotls.h"] + glob(
        ["deps/picotls/include/picotls/*.h"],
    ),
    copts = [
        "-std=c99",
        "-Wall",
        "-O2",
        "-g",
    ],
    includes = [
        "deps/picotls/include",
        "deps/picotls/include/picotls",
    ],
    linkstatic = True,
    local_defines = select({
        "@platforms//cpu:aarch64": ["URBIT_RUNTIME_CPU_AARCH64"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:private"],
    deps = [
        ":cifra",
        ":micro_ecc",
        "@openssl",
    ] + select({
        "@platforms//cpu:aarch64": ["@sse2neon"],
        "//conditions:default": [],
    }),
)

# See `deps/ssl-conservatory` in the `h2o` repo.
cc_library(
    name = "ssl_conservatory",
    hdrs = ["deps/ssl-conservatory/openssl/openssl_hostname_validation.h"],
    copts = ["-O3"],
    includes = ["deps/ssl-conservatory/openssl"],
    linkstatic = True,
    textual_hdrs = ["deps/ssl-conservatory/openssl/openssl_hostname_validation.c"],
    visibility = ["//visibility:private"],
)

# See `deps/yoml` in the `h2o` repo.
cc_library(
    name = "yoml",
    hdrs = glob(["deps/yoml/*.h"]),
    copts = ["-O3"],
    includes = ["deps/yoml"],
    linkstatic = True,
    visibility = ["//visibility:private"],
)

#
# PICOTLS DEPENDENCIES
#

# See `deps/picotls/deps/cifra` in the `h2o` repo.
cc_library(
    name = "cifra",
    srcs = [
        "deps/picotls/deps/cifra/src/aes.c",
        "deps/picotls/deps/cifra/src/sha256.c",
        "deps/picotls/deps/cifra/src/sha512.c",
        "deps/picotls/deps/cifra/src/chash.c",
        "deps/picotls/deps/cifra/src/hmac.c",
        "deps/picotls/deps/cifra/src/pbkdf2.c",
        "deps/picotls/deps/cifra/src/modes.c",
        "deps/picotls/deps/cifra/src/eax.c",
        "deps/picotls/deps/cifra/src/gf128.c",
        "deps/picotls/deps/cifra/src/blockwise.c",
        "deps/picotls/deps/cifra/src/cmac.c",
        "deps/picotls/deps/cifra/src/salsa20.c",
        "deps/picotls/deps/cifra/src/chacha20.c",
        "deps/picotls/deps/cifra/src/curve25519.c",
        "deps/picotls/deps/cifra/src/gcm.c",
        "deps/picotls/deps/cifra/src/cbcmac.c",
        "deps/picotls/deps/cifra/src/ccm.c",
        "deps/picotls/deps/cifra/src/sha3.c",
        "deps/picotls/deps/cifra/src/sha1.c",
        "deps/picotls/deps/cifra/src/poly1305.c",
        "deps/picotls/deps/cifra/src/norx.c",
        "deps/picotls/deps/cifra/src/chacha20poly1305.c",
        "deps/picotls/deps/cifra/src/drbg.c",
        "deps/picotls/deps/cifra/src/ocb.c",
    ] + glob(
        [
            "deps/picotls/deps/cifra/src/*.h",
            "deps/picotls/deps/cifra/src/ext/*.h",
        ],
        exclude = ["deps/picotls/deps/cifra/src/ext/handy.h"],
    ),
    hdrs = glob(
        [
            "deps/picotls/deps/cifra/src/*.h",
            "deps/picotls/deps/cifra/src/ext/*.h",
        ],
    ),
    copts = ["-O3"],
    includes = [
        "deps/picotls/deps/cifra/src",
        "deps/picotls/deps/cifra/src/ext",
    ],
    linkstatic = True,
    textual_hdrs = ["deps/picotls/deps/cifra/src/curve25519.tweetnacl.c"],
    visibility = ["//visibility:private"],
)

# See `deps/picotls/deps/micro-ecc` in the `h2o` repo.
cc_library(
    name = "micro_ecc",
    srcs = [
        "deps/picotls/deps/micro-ecc/types.h",
        "deps/picotls/deps/micro-ecc/uECC.c",
        "deps/picotls/deps/micro-ecc/uECC_vli.h",
    ],
    hdrs = ["deps/picotls/deps/micro-ecc/uECC.h"],
    copts = ["-O3"],
    includes = ["deps/picotls/deps/micro-ecc"],
    textual_hdrs = [
        "deps/picotls/deps/micro-ecc/asm_arm.inc",
        "deps/picotls/deps/micro-ecc/asm_arm_mult_square.inc",
        "deps/picotls/deps/micro-ecc/asm_arm_mult_square_umaal.inc",
        "deps/picotls/deps/micro-ecc/asm_avr.inc",
        "deps/picotls/deps/micro-ecc/asm_avr_mult_square.inc",
        "deps/picotls/deps/micro-ecc/curve-specific.inc",
        "deps/picotls/deps/micro-ecc/platform-specific.inc",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "h2o",
    # The `*.c` files below correspond to the files in `lib/` in
    # `LIB_SOURCE_FILES` in `CMakeLists.txt` in the `h2o` repo.
    #
    # Files in `deps/` in `LIB_SOURCE_FILES` are in their respective library
    # targets above.
    srcs = [
        "lib/common/cache.c",
        "lib/common/file.c",
        "lib/common/filecache.c",
        "lib/common/hostinfo.c",
        "lib/common/http1client.c",
        "lib/common/memcached.c",
        "lib/common/memory.c",
        "lib/common/multithread.c",
        "lib/common/serverutil.c",
        "lib/common/socket.c",
        "lib/common/socketpool.c",
        "lib/common/string.c",
        "lib/common/time.c",
        "lib/common/timeout.c",
        "lib/common/url.c",
        "lib/core/config.c",
        "lib/core/configurator.c",
        "lib/core/context.c",
        "lib/core/headers.c",
        "lib/core/logconf.c",
        "lib/core/proxy.c",
        "lib/core/request.c",
        "lib/core/token.c",
        "lib/core/util.c",
        "lib/handler/access_log.c",
        "lib/handler/chunked.c",
        "lib/handler/compress.c",
        "lib/handler/compress/gzip.c",
        "lib/handler/configurator/access_log.c",
        "lib/handler/configurator/compress.c",
        "lib/handler/configurator/errordoc.c",
        "lib/handler/configurator/expires.c",
        "lib/handler/configurator/fastcgi.c",
        "lib/handler/configurator/file.c",
        "lib/handler/configurator/headers.c",
        "lib/handler/configurator/headers_util.c",
        "lib/handler/configurator/http2_debug_state.c",
        "lib/handler/configurator/proxy.c",
        "lib/handler/configurator/redirect.c",
        "lib/handler/configurator/reproxy.c",
        "lib/handler/configurator/status.c",
        "lib/handler/configurator/throttle_resp.c",
        "lib/handler/errordoc.c",
        "lib/handler/expires.c",
        "lib/handler/fastcgi.c",
        "lib/handler/file.c",
        "lib/handler/headers.c",
        "lib/handler/headers_util.c",
        "lib/handler/http2_debug_state.c",
        "lib/handler/mimemap.c",
        "lib/handler/proxy.c",
        "lib/handler/redirect.c",
        "lib/handler/reproxy.c",
        "lib/handler/status.c",
        "lib/handler/status/durations.c",
        "lib/handler/status/events.c",
        "lib/handler/status/requests.c",
        "lib/handler/throttle_resp.c",
        "lib/http1.c",
        "lib/http2/cache_digests.c",
        "lib/http2/casper.c",
        "lib/http2/connection.c",
        "lib/http2/frame.c",
        "lib/http2/hpack.c",
        "lib/http2/http2_debug_state.c",
        "lib/http2/scheduler.c",
        "lib/http2/stream.c",
        "lib/tunnel.c",
    ] + glob(
        [
            "lib/core/*.h",
            "lib/handler/mimemap/*.h",
            "lib/handler/file/*.h",
            "lib/http2/*.h",
            "lib/common/socket/*.h",
        ],
    ),
    hdrs = ["include/h2o.h"] + glob(
        [
            "include/h2o/*.h",
            "include/h2o/socket/*.h",
        ],
    ),
    copts = [
        "-std=c99",
        "-g3",
        "-O2",
        "-pthread",
    ] + CC_WARNING_FLAGS,
    includes = [
        "include",
        "include/h2o",
        "include/h2o/socket",
    ],
    local_defines = [
        "H2O_USE_LIBUV",
        "H2O_USE_PICOTLS",
    ] + select({
        "@platforms//os:linux": ["_GNU_SOURCE"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        ":cloexec",
        ":golombset",
        ":klib",
        ":libgkc",
        ":libyrmcds",
        ":picohttpparser",
        ":picotls",
        ":ssl_conservatory",
        ":yoml",
        "@openssl",
        "@uv",
        "@zlib",
    ],
)
