load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "sigsegv",
    args = ["--jobs=`nproc`"],
    configure_options = [
        "--disable-shared",
        "--enable-static",
    ] + select({
        # Disable stack vma check, which reads from procfs, producing drastic
        # slowdowns.
        "@platforms//os:linux": ["--disable-stackvma"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libsigsegv.a"],
    visibility = ["//visibility:public"],
)
