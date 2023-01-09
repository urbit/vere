# This build file is derived from `libraries/liblmdb/Makefile` in the `lmdb`
# repo at https://github.com/LMDB/lmdb.

cc_library(
    name = "lmdb",
    srcs = [
        "mdb.c",
        "midl.c",
        "midl.h",
    ],
    hdrs = ["lmdb.h"],
    copts = [
        "-pthread",
        "-O2",
        "-g",
        "-W",
        "-Wall",
        "-Wno-unused-parameter",
        "-Wbad-function-cast",
        "-Wuninitialized",
    ],
    include_prefix = "lmdb",
    includes = ["."],
    local_defines = select({
        "@platforms//os:macos": ["URBIT_RUNTIME_OS_DARWIN"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)
