cc_library(
    name = "argon2",
    srcs = [
        "src/argon2.c",
        "src/blake2/blake2-impl.h",
        "src/blake2/blake2b.c",
        "src/core.c",
        "src/core.h",
        "src/encoding.c",
        "src/encoding.h",
        "src/thread.c",
        "src/thread.h",
    ] + select({
        # `opt.c` requires SSE instructions. See `Makefile` in the `argon2` repo
        # for the check used to determine whether to use `opt.c` or `ref.c`.
        "@platforms//cpu:x86_64": [
            "src/blake2/blamka-round-opt.h",
            "src/opt.c",
        ],
        "//conditions:default": [
            "src/blake2/blamka-round-ref.h",
            "src/ref.c",
        ],
    }),
    hdrs = [
        "include/argon2.h",
        "src/blake2/blake2.h",
    ],
    copts = [
        "-std=c89",
        "-O3",
        "-Wall",
        "-g",
        "-Isrc",
        "-DARGON2_NO_THREADS",
    ],
    includes = [
        "include",
        "src/blake2",
    ],
    visibility = ["//visibility:public"],
)
