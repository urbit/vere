load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "tool_path")

def _cc_toolchain_config_impl(ctx):
    # See
    # https://bazel.build/rules/lib/cc_common#create_cc_toolchain_config_info.
    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        # See https://bazel.build/docs/cc-toolchain-config-reference#features.
        features = [],
        cxx_builtin_include_directories = ctx.attr.sys_includes,
        toolchain_identifier = ctx.attr.toolchain_identifier,
        target_system_name = ctx.attr.target_system_name,
        target_cpu = ctx.attr.target_cpu,
        target_libc = ctx.attr.target_libc,
        compiler = ctx.attr.compiler,
        abi_version = ctx.attr.abi_version,
        abi_libc_version = ctx.attr.abi_libc_version,
        tool_paths = [
            tool_path(
                name = "gcc",
                path = ctx.attr.cc,
            ),
            tool_path(
                name = "ld",
                path = ctx.attr.ld,
            ),
            tool_path(
                name = "ar",
                path = ctx.attr.ar,
            ),
            tool_path(
                name = "cpp",
                path = ctx.attr.cpp,
            ),
            tool_path(
                name = "gcov",
                path = ctx.attr.gcov,
            ),
            tool_path(
                name = "nm",
                path = ctx.attr.nm,
            ),
            tool_path(
                name = "objdump",
                path = ctx.attr.objdump,
            ),
            tool_path(
                name = "strip",
                path = ctx.attr.strip,
            ),
        ],
    )

cc_toolchain_config = rule(
    implementation = _cc_toolchain_config_impl,
    attrs = {
        # Required.
        "ar": attr.string(mandatory = True),
        "cc": attr.string(mandatory = True),
        "compiler": attr.string(mandatory = True),
        "ld": attr.string(mandatory = True),
        "target_cpu": attr.string(mandatory = True),
        "toolchain_identifier": attr.string(mandatory = True),
        # Optional.
        "abi_libc_version": attr.string(default = "unknown"),
        "abi_version": attr.string(default = "unknown"),
        "cpp": attr.string(default = "/bin/false"),
        "gcov": attr.string(default = "/bin/false"),
        "nm": attr.string(default = "/bin/false"),
        "objdump": attr.string(default = "/bin/false"),
        "strip": attr.string(default = "/bin/false"),
        "sys_includes": attr.string_list(default = []),
        "target_libc": attr.string(default = "unknown"),
        "target_system_name": attr.string(default = "unknown"),
    },
    provides = [CcToolchainConfigInfo],
)
