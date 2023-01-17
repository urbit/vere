load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "uv",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--disable-shared",
    ] + select({
        # The `--host` flag is not well-documented by the docs in the libuv
        # repo, but the following commands produce an AArch64 library:
        # ```
        # $ CC=path/to/aarch64-linux-musl-gcc ./configure --prefix=/tmp --host=aarch64-linux-musl
        # $ make install
        # ```
        #
        # This can be verified with:
        # ```
        # $ path/to/aarch64-linux-musl-objdump -d /tmp/lib/libuv.a
        # ```
        "@//:linux_aarch64": ["--host=aarch64-linux-musl"],
        "@//:linux_x86_64": ["--host=x86_64-linux-musl"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libuv.a"],
    targets = ["install"],
    visibility = ["//visibility:public"],
)
