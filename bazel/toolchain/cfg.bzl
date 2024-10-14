load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
    "variable_with_value",
)

def _cc_toolchain_config_impl(ctx):
    ar_flags = feature(
        name = "archiver_flags",
        flag_sets = [
            flag_set(
                actions = [ACTION_NAMES.cpp_link_static_library],
                flag_groups = [
                    flag_group(flags = [ctx.attr.ar_flags]),
                    flag_group(
                        flags = ["%{output_execpath}"],
                        expand_if_available = "output_execpath",
                    ),
                ],
            ),
            flag_set(
                actions = [ACTION_NAMES.cpp_link_static_library],
                flag_groups = [
                    flag_group(
                        iterate_over = "libraries_to_link",
                        flag_groups = [
                            flag_group(
                                flags = ["%{libraries_to_link.name}"],
                                expand_if_equal = variable_with_value(
                                    name = "libraries_to_link.type",
                                    value = "object_file",
                                ),
                            ),
                            flag_group(
                                flags = ["%{libraries_to_link.object_files}"],
                                iterate_over = "libraries_to_link.object_files",
                                expand_if_equal = variable_with_value(
                                    name = "libraries_to_link.type",
                                    value = "object_file_group",
                                ),
                            ),
                        ],
                        expand_if_available = "libraries_to_link",
                    ),
                ],
            ),
        ],
    )
    features = [ar_flags]

    if len(ctx.attr.cc_flags) > 0:
        cc_flags = feature(
            name = "cc_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.c_compile],
                    flag_groups = [flag_group(flags = ctx.attr.cc_flags)],
                ),
            ],
        )
        features.append(cc_flags)


    if len(ctx.attr.ld_flags) > 0:
        ld_flags = feature(
            name = "ld_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [
                        ACTION_NAMES.cpp_link_dynamic_library,
                        ACTION_NAMES.cpp_link_executable,
                        ACTION_NAMES.cpp_link_nodeps_dynamic_library,
                    ],
                    flag_groups = [flag_group(flags = ctx.attr.ld_flags)],
                ),
            ],
        )
        features.append(ld_flags)

    # See
    # https://bazel.build/rules/lib/cc_common#create_cc_toolchain_config_info.
    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        # Replace `{compiler_version}` in all include paths with the value of
        # the `compiler_version` label.
        cxx_builtin_include_directories = [
            path.format(compiler_version = ctx.attr.compiler_version[BuildSettingInfo].value)
            for path in ctx.attr.sys_includes
        ],
        features = features,
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
        "compiler_version": attr.label(mandatory = True),
        "ld": attr.string(mandatory = True),
        "target_cpu": attr.string(mandatory = True),
        "toolchain_identifier": attr.string(mandatory = True),
        # Optional.
        "abi_libc_version": attr.string(default = "unknown"),
        "abi_version": attr.string(default = "unknown"),
        "ar_flags": attr.string(default = "rcsD"),
        "cc_flags": attr.string_list(default = []),
        "cpp": attr.string(default = "/bin/false"),
        "gcov": attr.string(default = "/bin/false"),
        "ld_flags": attr.string_list(default = []),
        "nm": attr.string(default = "/bin/false"),
        "objdump": attr.string(default = "/bin/false"),
        "strip": attr.string(default = "/bin/false"),
        "sys_includes": attr.string_list(default = []),
        "target_libc": attr.string(default = "unknown"),
        "target_system_name": attr.string(default = "unknown"),
    },
    provides = [CcToolchainConfigInfo],
)
