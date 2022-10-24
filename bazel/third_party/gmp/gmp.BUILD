load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(
        ["**"],
        allow_empty = False,
    ),
)

# TODO: check windows build.
configure_make(
    name = "gmp",
    args = ["--jobs=`nproc`"],
    configure_options = ["--disable-shared"],
    lib_source = ":all",
    out_static_libs = ["libgmp.a"],
    visibility = ["//visibility:public"],
)
