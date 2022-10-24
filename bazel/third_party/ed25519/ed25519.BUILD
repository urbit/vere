cc_library(
    name = "ed25519",
    srcs = glob(
        [
            "src/*.c",
            "src/*.h",
        ],
        allow_empty = False,
        exclude = ["src/ed25519.h"],
    ),
    hdrs = ["src/ed25519.h"],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
