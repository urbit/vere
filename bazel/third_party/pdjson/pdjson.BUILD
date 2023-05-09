# This build file is derived from `Makefile` in the `pdjson` repo at
# https://github.com/skeeto/pdjson.

cc_library(
    name = "pdjson",
    srcs = ["pdjson.c"],
    hdrs = ["pdjson.h"],
    copts = [
        "-std=c99",
        "-pedantic",
        "-Wall",
        "-Wextra",
        "-Wno-missing-field-initializers"
    ],
    visibility = ["//visibility:public"]
)
