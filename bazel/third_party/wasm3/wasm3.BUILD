# This build file is derived form `CMakeLists.txt` in the `wasm3` repo at
# https://github.com/wasm3/wasm3.
 
cc_library(
    name = "wasm3",
    srcs = glob(["source/*.c"], exclude=["source/m3_api*.c"]),
    hdrs = glob(["source/*.h"], exclude=["source/m3_api*.h"]),
    includes = ["source/", "source/extra/"],
    copts = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wparentheses",
        "-Wundef",
        "-Wpointer-arith",
        "-Wstrict-aliasing=2",
        "-Werror=implicit-function-declaration",
        # Add more compiler flags as needed
    ],
    visibility = ["//visibility:public"],
    deps = ["@softfloat"]
)

