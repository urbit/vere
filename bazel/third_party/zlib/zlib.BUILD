load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "zlib",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_options = ["--static"],
    copts = ["-O3"],
    lib_source = ":all",
    out_static_libs = ["libz.a"],
    visibility = ["//visibility:public"],
)
