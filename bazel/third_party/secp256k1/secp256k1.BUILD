load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "secp256k1",
    args = ["--jobs=`nproc`"],
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--disable-shared",
        "--enable-module-recovery",
        "--enable-module-schnorrsig",
        "--enable-static",
    ] + select({
        # Native compilation on linux-arm64 isn't supported.
        "@//:linux_arm64": ["--host=aarch64-linux-gnu"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libsecp256k1.a"],
    visibility = ["//visibility:public"],
)
