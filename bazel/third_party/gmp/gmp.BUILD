load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# TODO: check windows build.
configure_make(
    name = "gmp",
    args = ["--jobs=`nproc`"],
    configure_options = [
        "--disable-shared",
    ] + select({
        # Native compilation on linux-arm64 isn't supported.
        "@//:linux_arm64": ["--host=aarch64-linux-gnu"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libgmp.a"],
    visibility = ["//visibility:public"],
)
