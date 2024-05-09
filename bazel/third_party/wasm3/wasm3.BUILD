# This build file is derived form `CMakeLists.txt` in the `wasm3` repo at
# https://github.com/wasm3/wasm3.
 
cc_library(
    name = "wasm3",
    srcs = glob(["**/*.c"], exclude=["test/**/*.c", "platforms/android/**/*.c", "platforms/emscripten/**/*.c", "platforms/emscripten_lib/**/*.c", "platforms/embedded/**/*.c", "platforms/cpp/**/*.c", "platforms/ios/**/*.c"]),
    hdrs = glob(["**/*.h"], exclude=["test/**/*.h", "platforms/android/**/*.h", "platforms/emscripten/**/*.h", "platforms/emscripten_lib/**/*.h", "platforms/embedded/**/*.h", "platforms/cpp/**/*.h", "platforms/ios/**/*.h"]),
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
)

