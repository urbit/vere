load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# TODO: check windows build.
configure_make(
    name = "uv",
    args = ["--jobs=`nproc`"],
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--disable-shared",
    ],
    lib_source = ":all",
    out_static_libs = ["libuv.a"],
    targets = ["install"],
    visibility = ["//visibility:public"],
)
