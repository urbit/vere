load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(
        ["**"],
        allow_empty = False,
    ),
)

configure_make(
    name = "sigsegv",
    args = ["--jobs=`nproc`"],
    configure_options = [
        "--disable-shared",
        "--enable-static",
    ],
    lib_source = ":all",
    out_static_libs = ["libsigsegv.a"],
    visibility = ["//visibility:public"],
)
