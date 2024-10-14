# This build file is derived from `makefile` in the `murmur3` repo at
# https://github.com/PeterScott/murmur3.

cc_library(
    name = "murmur3",
    srcs = ["murmur3.c"],
    hdrs = ["murmur3.h"],
    copts = [
        "-O3",
        "-Wall",
    ] + select({
        # TODO: use selects.with_or() from skylib once it's available.
        "@platforms//os:macos": [
            "-fPIC",
            "-c",
        ],
        "//conditions:default": [],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
)
