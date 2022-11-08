load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "zlib",
    args = ["--jobs=`nproc`"],
    configure_options = ["--static"],
    lib_source = ":all",
    out_static_libs = ["libz.a"],
    visibility = ["//visibility:public"],
)
