cc_library(
    name = "ed25519",
    srcs = glob(
        [
            "src/*.c",
            "src/*.h",
        ],
        exclude = ["src/ed25519.h"],
    ),
    hdrs = ["src/ed25519.h"],
    copts = ["-O3"],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
