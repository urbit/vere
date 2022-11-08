load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# TODO: use configure_make_variant() to select nmake toolchain on windows?
configure_make(
    name = "openssl",
    args = ["--jobs=`nproc`"],
    configure_command = select({
        "@platforms//os:windows": "Configure",
        "//conditions:default": "config",
    }),
    configure_options = [
        "no-shared",
    ] + select({
        "@platforms//os:windows": ["mingw64"],
        "//conditions:default": [],
    }),
    configure_prefix = select({
        "@platforms//os:windows": "perl",
        "//conditions:default": "",
    }),
    lib_source = ":all",
    out_static_libs = [
        "libssl.a",
        "libcrypto.a",
    ],
    targets = [
        "build_libs",
        "install_dev",
    ],
    visibility = ["//visibility:public"],
)
