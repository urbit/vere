load("@rules_rust//rust:defs.bzl", "rust_static_library")
load("@crate_index//:defs.bzl", "all_crate_deps")

rust_static_library(
    name = "wasmer",
    srcs = glob(
        [
            "**/*.rs",
        ],
    ),
    # crate_type = "staticlib",
    edition = "2021",
    crate_features = ["singlepass"],
    deps = all_crate_deps(),
    proc_macro_deps = all_crate_deps(proc_macro = True),
    visibility = ["//visibility:public"],
)

exports_files(
    glob([
        "**/*.rs",
        "*.rs",
    ])
)
