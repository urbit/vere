cc_library(
    name = "wasmer-darwin-amd64",
    hdrs = glob(["include/*.h"]),
    srcs = ["lib/libwasmer.a"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "wasmer-darwin-arm64",
    hdrs = glob(["include/*.h"]),
    srcs = ["lib/libwasmer.a"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "wasmer-linux-amd64",
    hdrs = glob(["include/*.h"]),
    srcs = ["lib/libwasmer.a"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "wasmer-linux-aarch64",
    hdrs = glob(["include/*.h"]),
    srcs = ["lib/libwasmer.a"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)

alias(
    name = "wasmer",
    actual = select({
        "@//:macos_aarch64": ":wasmer-darwin-arm64",
        "@//:macos_x86_64": ":wasmer-darwin-amd64",
        "@//:linux_aarch64": ":wasmer-linux-aarch64",
        "@//:linux_x86_64": ":wasmer-linux-amd64",
    }),
)
