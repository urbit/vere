load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "sigsegv",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_options = [
        "--disable-shared",
        "--enable-static",
    ] + select({
        # Disable stack vma check, which reads from procfs, producing drastic
        # slowdowns.
        "@platforms//os:linux": ["--disable-stackvma"],
        "//conditions:default": [],
    }) + select({
        # To build for `linux-aarch64` on `linux-aarch64`, replace
        # `--host=x86_64-linux --target=aarch64-linux-musl` below with
        # `--host=aarch64-linux-musl`.
        "@//:linux_aarch64": [
            "--host=x86_64-linux",
            "--target=aarch64-linux-musl",
        ],
        "@//:linux_x86_64": ["--host=x86_64-linux-musl"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libsigsegv.a"],
    visibility = ["//visibility:public"],
)
