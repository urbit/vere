load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "secp256k1",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--disable-shared",
        "--enable-module-recovery",
        "--enable-module-schnorrsig",
        "--enable-static",
    ] + select({
        "@//:linux_aarch64": ["--host=aarch64-linux-musl"],
        "@//:linux_x86_64": ["--host=x86_64-linux-musl"],
        "//conditions:default": [],
    }),
    copts = ["-O3"],
    lib_source = ":all",
    out_static_libs = ["libsecp256k1.a"],
    visibility = ["//visibility:public"],
)
