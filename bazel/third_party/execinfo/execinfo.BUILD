# This build file is derived from `Makefile` in the `libexecinfo` package at
# http://distcache.freebsd.org/local-distfiles/itetcu/libexecinfo-1.1.tar.bz2
# and the makefile patch in the alpinelinux package at
# https://pkgs.alpinelinux.org/packages?name=libexecinfo&branch=v3.12

cc_library(
    name = "execinfo",
    srcs = [
        "stacktraverse.c",
        "stacktraverse.h",
        "execinfo.c",
        "execinfo.h",
    ],
    hdrs = ["execinfo.h"],
    copts = [
        "-O2",
        "-pipe",
        "-fno-strict-aliasing",
        "-fstack-protector",
        # "-Wall",
        "-fno-omit-frame-pointer",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
