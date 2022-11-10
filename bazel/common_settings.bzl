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
