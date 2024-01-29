load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

make(
    name = "natpmp",
    out_static_libs = ["libnatpmp.a"],
    out_lib_dir = "/usr/lib",
    out_include_dir	 = "/usr/include",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    copts = ["-O3"],
    lib_source = ":all",
    visibility = ["//visibility:public"],
    postfix_script = "cp $BUILD_TMPDIR/natpmp_declspec.h $INSTALLDIR/usr/include/natpmp_declspec.h",
)