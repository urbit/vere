# For more, see https://bazel.build/extending/config and
# https://github.com/bazelbuild/bazel-skylib/blob/main/rules/common_settings.bzl.
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

def _impl(ctx):
    return BuildSettingInfo(value = ctx.build_setting_value)

string_flag = rule(
    implementation = _impl,
    build_setting = config.string(flag = True),
    doc = "A string-typed build setting that can be set on the command line",
)

def vere_library(copts = [], linkopts = [], **kwargs):
  native.cc_library(
    copts = copts + select({
        "//:debug": ["-O0", "-g3", "-DC3DBG", "-fdebug-compilation-dir=."],
        "//conditions:default": ["-O3"]
    }) + select({
        "//:lto": ['-flto'],
        "//:thinlto": ['-flto=thin'],
        "//conditions:default": []
    }) + select({
        # Don't include source level debug info on macOS. See
        # https://github.com/urbit/urbit/issues/5561 and
        # https://github.com/urbit/vere/issues/131.
        "//:debug": [],
        "@platforms//os:linux": ["-g"],
        "//conditions:default": [],
    }),
    linkopts = linkopts + ['-g'] + select({
        "//:lto": ['-flto'],
        "//:thinlto": ['-flto=thin'],
        "//conditions:default": []
    }),
    **kwargs,
  )

def vere_binary(copts = [], linkopts = [], **kwargs):
  native.cc_binary(
    copts = copts + select({
        "//:debug": ["-O0", "-g3", "-DC3DBG", "-fdebug-compilation-dir=."],
        "//conditions:default": ["-O3"]
    }) + select({
        "//:lto": ['-flto'],
        "//:thinlto": ['-flto=thin'],
        "//conditions:default": []
    }) + select({
        "//:debug": [],
        "@platforms//os:linux": ["-g"],
        "//conditions:default": [],
    }),
    linkopts = linkopts + ['-g'] + select({
        "//:lto": ['-flto'],
        "//:thinlto": ['-flto=thin'],
        "//conditions:default": []
    }),
    **kwargs,
  )
