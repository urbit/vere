genrule(
    name = "config",
    srcs = ["config.h.in"],
    outs = ["config.h"],
    # The options in config.h.in only affect libaes_siv's testing behavior, so
    # it's sufficient to generate an empty config.h.
    cmd = "touch $@",
)

cc_library(
    name = "aes_siv",
    srcs = [
        "aes_siv.c",
        "config.h",
    ],
    hdrs = ["aes_siv.h"],
    copts = ["-O3"],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["@openssl"],
)
