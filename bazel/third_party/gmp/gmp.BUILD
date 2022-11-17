load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# TODO: check windows build.
configure_make(
    name = "gmp",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_options = [
        "--disable-shared",
    ] + select({
        # Native compilation on linux-arm64 isn't supported.
        "@//:linux_arm64": ["--host=aarch64-linux-gnu"],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    # NOTE: This prevents warnings on `macos-x86_64`.
    # See: https://github.com/bazelbuild/bazel/issues/16413.
    # linkopts = select({
    #     "@//:macos_x86_64": ["-Wl,-no_fixup_chains"],
    #     "//conditions:default": [],
    # }),
    out_static_libs = ["libgmp.a"],
    visibility = ["//visibility:public"],
)
