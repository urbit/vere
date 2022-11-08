# This build file is derived from `Makefile` in the `scrypt` repo at
# https://github.com/technion/libscrypt.

cc_library(
    name = "scrypt",
    srcs = [
        "b64.c",
        "b64.h",
        "crypto-mcf.c",
        "crypto-scrypt-saltgen.c",
        "crypto_scrypt-check.c",
        "crypto_scrypt-hash.c",
        "crypto_scrypt-nosse.c",
        "sha256.c",
        "sha256.h",
        "slowequals.c",
        "slowequals.h",
        "sysendian.h",
    ],
    hdrs = ["libscrypt.h"],
    copts = [
        "-O2",
        "-Wall",
        "-g",
        "-fstack-protector",
    ],
    includes = ["."],
    local_defines = ["_FORTIFY_SOURCE=2"],
    visibility = ["//visibility:public"],
)
