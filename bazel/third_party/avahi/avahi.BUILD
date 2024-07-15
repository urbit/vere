load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

cc_library(
    name = "dns-sd",
    hdrs = ["dns_sd.h"],
    visibility = ["//visibility:public"],
)

configure_make(
    name = "avahi",
    args = select({
        "@platforms//os:macos": ["--jobs=`sysctl -n hw.logicalcpu`"],
        "//conditions:default": ["--jobs=`nproc`"],
    }),
    configure_options = ["--with-dbus-system-address='unix:path=/var/run/dbus/system_bus_socket' --with-xml=none --disable-libevent --disable-glib --disable-gobject --disable-gdbm --disable-qt3 --disable-qt4 --disable-qt5 --disable-gtk --disable-gtk3 --disable-mono --disable-monodoc --disable-python --disable-libdaemon --enable-compat-libdns_sd --disable-rpath --with-distro=none"],
    lib_source = ":all",
    # out_include_dir = "avahi-compat-libdns_sd",
    deps = ["@dbus"],
    configure_in_place = True,
    autogen = True,
    autoconf = True,
    autogen_command = "bootstrap.sh",
    out_static_libs = ["libdns_sd.a", "libavahi-client.a", "libavahi-common.a"],
    visibility = ["//visibility:public"],
)
