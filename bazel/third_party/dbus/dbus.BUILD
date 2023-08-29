load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "dbus",
    lib_name = "libdbus-1",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    copts = ["-O3"],
    configure_options = ["--disable-selinux"],
    lib_source = ":all",
    configure_in_place = True,
    deps = ["@expat"],
    visibility = ["//visibility:public"],
)
