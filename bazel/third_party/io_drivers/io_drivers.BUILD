load("@crate_index_io_drivers//:defs.bzl", "all_crate_deps")
load("@rules_rust//rust:defs.bzl", "rust_static_library")

rust_static_library(
    name = "io_drivers",
    srcs = glob(["src/**/*.rs"]),
    crate_features = ["http-client"],
    visibility = ["//visibility:public"],
    deps = all_crate_deps(),
)
