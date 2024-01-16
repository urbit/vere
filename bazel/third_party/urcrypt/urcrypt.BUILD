load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "urcrypt",
    autogen = True,
    configure_in_place = True,
    configure_options = [
        "--disable-shared",
    ],
    copts = [
        "-Wall",
        "-g",
        "-O3",
    ],
    deps = [
        "@aes_siv",
        "@openssl",
        "@secp256k1"
    ],
    lib_source = ":all",
    out_static_libs = ["liburcrypt.a"],
    visibility = ["//visibility:public"],
)
