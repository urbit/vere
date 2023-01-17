load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "gmp",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_options = [
        "--disable-shared",
        # NOTE: --with-pic is required to build PIE binaries on macos_x86_64,
        # but we leave it in for all builds as a precaution.
        "--with-pic",
    ] + select({
        # From INSTALL in the gmp repo: "If you want to *use* a cross compiler
        # that generates code for a platform different from the build platform,
        # you should specify the 'host' platform (i.e., that on which the
        # generated programs will eventually run) with `--host=TYPE`."
        "@//:linux_aarch64": ["--host=aarch64-linux-musl"],
        "@//:linux_x86_64": ["--host=x86_64-linux-musl"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_static_libs = ["libgmp.a"],
    visibility = ["//visibility:public"],
)
