load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(
        ["**"],
        allow_empty = False,
    ),
)

# TODO: check windows build.
configure_make(
    name = "curl",
    args = ["--jobs=`nproc`"],
    # We disable unneeded features.
    # TODO: double check that all disabled features below are in fact unneeded.
    configure_options = [
        "--disable-alt-svc",
        "--disable-ares",
        "--disable-cookies",
        "--disable-crypto-auth",
        "--disable-dateparse",
        "--disable-dnsshuffle",
        "--disable-doh",
        "--disable-get-easy-options",
        "--disable-hsts",
        "--disable-http-auth",
        "--disable-ipv6",
        "--disable-ldap",
        "--disable-libcurl-option",
        "--disable-manual",
        "--disable-shared",
        "--disable-netrc",
        "--disable-ntlm-wb",
        "--disable-progress-meter",
        "--disable-proxy",
        "--disable-pthreads",
        "--disable-socketpair",
        "--disable-threaded-resolver",
        "--disable-tls-srp",
        "--disable-unix-sockets",
        "--disable-verbose",
        "--disable-versioned-symbols",
        "--enable-static",
        # Use our openssl, not the system's openssl.
        "--with-openssl=$(URBIT_RUNTIME_OPENSSL)",
        "--without-brotli",
        "--without-libidn2",
        "--without-libpsl",
        "--without-librtmp",
        "--without-nghttp2",
        "--without-ngtcp2",
        "--without-ssl",
        "--without-zlib",
        "--without-zstd",
    ],
    env = {
        "URBIT_RUNTIME_OPENSSL": "$(GENDIR)/external/openssl/openssl",
    },
    lib_source = ":all",
    out_static_libs = ["libcurl.a"],
    visibility = ["//visibility:public"],
    deps = ["@openssl"],
)
