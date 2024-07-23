load("@rules_rust//rust:defs.bzl", "rust_library")
load("@crate_index//:defs.bzl", "all_crate_deps")
load("@rules_rust//crate_universe:defs.bzl", "crates_lockfile")

filegroup(
    name = "all",
    srcs = glob(["lib/c-api/**/*.rs"]),
)

rust_library(
    name = "wasmer",
    srcs = [":all"],
    crate_root = "lib/c-api/src/lib.rs",
    crate_type = "staticlib",
    edition = "2021",
    crate_features = ["singlepass"],
    deps = all_crate_deps(),
    proc_macro_deps = all_crate_deps(proc_macro = True),
    visibility = ["//visibility:public"],
)

crates_lockfile(
    name = "Cargo_lock",
    generator = "@crate_index//:cargo_lock_generator",
)
