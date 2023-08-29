load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "expat",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    # configure_options = ["--with-xml=none --disable-libevent --disable-glib --disable-gobject --disable-gdbm --disable-qt3 --disable-qt4 --disable-qt5 --disable-gtk --disable-gtk3 --disable-mono --disable-monodoc --disable-python --enable-compat-libdns_sd"],
    copts = ["-O3"],
    lib_source = ":all",
    # deps = ["@dbus"],
    out_static_libs = ["libexpat.a"],
    visibility = ["//visibility:public"],
)
