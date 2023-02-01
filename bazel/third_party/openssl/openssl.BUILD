load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "openssl",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_command = select({
        "@//:linux_aarch64": "Configure",
        "//conditions:default": "config",
    }),
    configure_options = [
        "no-shared",
    ] + select({
        "@//:linux_aarch64": [
            "linux-aarch64",
            # Native compilation on linux-aarch64 isn't supported. The prefix is
            # empty because the configure script detects an absolute path to the
            # aarch64-linux-gnu-gcc instead of just the binary name. This is
            # presumably because of the Bazel toolchain configuration but is not
            # an issue.
            "--cross-compile-prefix=",
        ],
        "//conditions:default": [],
    }),
    copts = ["-O3"],
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
