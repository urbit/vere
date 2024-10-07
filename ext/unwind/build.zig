const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const dep_c = b.dependency("unwind", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "unwind",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    lib.root_module.addCMacro("HAVE_CONFIG_H", "");
    lib.root_module.addCMacro("_GNU_SOURCE", "");
    lib.root_module.addCMacro("NDEBUG", "");
    lib.root_module.addCMacro("__EXTENSIONS__", "");

    const config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{
        .CONFIG_BLOCK_SIGNALS = "",
        .CONFIG_DEBUG_FRAME = "",
        .CONFIG_MSABI_SUPPORT = null,
        .CONFIG_WEAK_BACKTRACE = 1,
        .CONSERVATIVE_CHECKS = 1,
        .HAVE_ASM_PTRACE_H = 1,
        .HAVE_ASM_PTRACE_OFFSETS_H = null,
        .HAVE_ASM_VSYSCALL_H = null,
        .HAVE_BYTESWAP_H = 1,
        .HAVE_DECL_PTRACE_CONT = 1,
        .HAVE_DECL_PTRACE_POKEDATA = 1,
        .HAVE_DECL_PTRACE_POKEUSER = 1,
        .HAVE_DECL_PTRACE_SETREGSET = 1,
        .HAVE_DECL_PTRACE_SINGLESTEP = 1,
        .HAVE_DECL_PTRACE_SYSCALL = 1,
        .HAVE_DECL_PTRACE_TRACEME = 1,
        .HAVE_DECL_PT_CONTINUE = 1,
        .HAVE_DECL_PT_GETFPREGS = 0,
        .HAVE_DECL_PT_GETREGS = 0,
        .HAVE_DECL_PT_IO = 0,
        .HAVE_DECL_PT_STEP = 1,
        .HAVE_DECL_PT_SYSCALL = 1,
        .HAVE_DECL_PT_TRACE_ME = 1,
        .HAVE_DLFCN_H = 1,
        .HAVE_DLMODINFO = null,
        .HAVE_DL_ITERATE_PHDR = 1,
        .HAVE_DL_PHDR_REMOVALS_COUNTER = null,
        .HAVE_ELF_FPREGSET_T = 1,
        .HAVE_ELF_H = 1,
        .HAVE_ENDIAN_H = 1,
        .HAVE_EXECINFO_H = 1,
        .HAVE_EXECVPE = 1,
        .HAVE_GETUNWIND = null,
        .HAVE_IA64INTRIN_H = null,
        .HAVE_INTTYPES_H = 1,
        .HAVE_LINK_H = 1,
        .HAVE_LZMA = null,
        .HAVE_MEMORY_H = 1,
        .HAVE_MINCORE = 1,
        .HAVE_PIPE2 = 1,
        .HAVE_PROCFS_STATUS = null,
        .HAVE_SIGALTSTACK = 1,
        .HAVE_SIGNAL_H = 1,
        .HAVE_STDINT_H = 1,
        .HAVE_STDLIB_H = 1,
        .HAVE_STRINGS_H = 1,
        .HAVE_STRING_H = 1,
        .HAVE_STRUCT_DL_PHDR_INFO_DLPI_SUBS = 1,
        .HAVE_STRUCT_ELF_PRSTATUS = 1,
        .HAVE_STRUCT_PRSTATUS = null,
        .HAVE_SYS_ELF_H = 1,
        .HAVE_SYS_ENDIAN_H = null,
        .HAVE_SYS_LINK_H = null,
        .HAVE_SYS_PARAM_H = 1,
        .HAVE_SYS_PROCFS_H = 1,
        .HAVE_SYS_PTRACE_H = 1,
        .HAVE_SYS_STAT_H = 1,
        .HAVE_SYS_SYSCALL_H = 1,
        .HAVE_SYS_TYPES_H = 1,
        .HAVE_SYS_UC_ACCESS_H = null,
        .HAVE_TTRACE = null,
        .HAVE_UNISTD_H = 1,
        .HAVE_ZLIB = null,
        .HAVE__BUILTIN_UNREACHABLE = 1,
        .HAVE__BUILTIN___CLEAR_CACHE = 1,
        .HAVE___CACHE_PER_THREAD = null,
        .LT_OBJDIR = ".libs/",
        .PACKAGE = "libunwind",
        .PACKAGE_BUGREPORT = "https://github.com/libunwind/libunwind",
        .PACKAGE_NAME = "libunwind",
        .PACKAGE_STRING = "libunwind 1.8.1",
        .PACKAGE_TARNAME = "libunwind",
        .PACKAGE_URL = "",
        .PACKAGE_VERSION = "1.8.1",
        .SIZEOF_OFF_T = 8,
        .STDC_HEADERS = 1,
        .VERSION = "1.8.1",
        .@"const" = null,
        .@"inline" = null,
        .size_t = null,
    });

    const libunwind_h = b.addConfigHeader(.{
        .style = .{
            .cmake = dep_c.path("include/libunwind.h.in"),
        },
        .include_path = "libunwind.h",
    }, .{ .arch = @tagName(t.cpu.arch) });

    const libunwind_i_h = b.addConfigHeader(.{
        .style = .{
            .cmake = dep_c.path("include/tdep/libunwind_i.h.in"),
        },
        .include_path = "tdep/libunwind_i.h",
    }, .{ .arch = @tagName(t.cpu.arch) });

    const libunwind_common_h = b.addConfigHeader(.{
        .style = .{
            .cmake = dep_c.path("include/libunwind-common.h.in"),
        },
        .include_path = "libunwind-common.h",
    }, .{ .PKG_MAJOR = 1, .PKG_MINOR = 8, .PKG_EXTRA = 1 });

    lib.addConfigHeader(config_h);
    lib.addConfigHeader(libunwind_h);
    lib.addConfigHeader(libunwind_i_h);
    lib.addConfigHeader(libunwind_common_h);
    lib.addIncludePath(dep_c.path("src"));
    lib.addIncludePath(dep_c.path("include"));
    if (t.cpu.arch.isAARCH64())
        lib.addIncludePath(dep_c.path("include/tdep-aarch64"));
    if (t.cpu.arch == .x86_64)
        lib.addIncludePath(dep_c.path("include/tdep-x86_64"));

    var srcs = std.ArrayList([]const u8).init(b.allocator);
    defer srcs.deinit();

    try srcs.appendSlice(&.{
        // libunwind.la
        "os-linux.c",
        "dl-iterate-phdr.c",
        "mi/init.c",
        "mi/flush_cache.c",
        "mi/mempool.c",
        "mi/strerror.c",
        "mi/backtrace.c",
        "mi/dyn-cancel.c",
        "mi/dyn-info-list.c",
        "mi/dyn-register.c",
        "mi/Laddress_validator.c",
        "mi/Ldestroy_addr_space.c",
        "mi/Ldyn-extract.c",
        "mi/Lfind_dynamic_proc_info.c",
        "mi/Lget_accessors.c",
        "mi/Lget_fpreg.c",
        "mi/Lset_fpreg.c",
        "mi/Lget_proc_info_by_ip.c",
        "mi/Lget_proc_name.c",
        "mi/Lget_reg.c",
        "mi/Lput_dynamic_unwind_info.c",
        "mi/Lset_cache_size.c",
        "mi/Lset_caching_policy.c",
        "mi/Lset_iterate_phdr_function.c",
        "mi/Lset_reg.c",
        "mi/Lget_elf_filename.c",
        // libunwind-dwarf-local.la
        "dwarf/Lexpr.c",
        "dwarf/Lfde.c",
        "dwarf/Lfind_proc_info-lsb.c",
        "dwarf/Lfind_unwind_table.c",
        "dwarf/Lget_proc_info_in_range.c",
        "dwarf/Lparser.c",
        "dwarf/Lpe.c",
        // libunwind-dwarf-common.la
        "dwarf/global.c",
        // libunwind-elf64.la
        "elf64.c",
    });

    if (t.cpu.arch.isAARCH64())
        try srcs.appendSlice(&.{
            "aarch64/is_fpreg.c",
            "aarch64/regname.c",
            "aarch64/Los-linux.c",
            "aarch64/getcontext.S",
            "aarch64/Lapply_reg_state.c",
            "aarch64/Lcreate_addr_space.c",
            "aarch64/Lget_proc_info.c",
            "aarch64/Lget_save_loc.c",
            "aarch64/Lglobal.c",
            "aarch64/Linit.c",
            "aarch64/Linit_local.c",
            "aarch64/Linit_remote.c",
            "aarch64/Lis_signal_frame.c",
            "aarch64/Lregs.c",
            "aarch64/Lreg_states_iterate.c",
            "aarch64/Lresume.c",
            "aarch64/Lstash_frame.c",
            "aarch64/Lstep.c",
            "aarch64/Ltrace.c",
            // "aarch64/longjmp.S",
            // "aarch64/setcontext.S",
            // "aarch64/siglongjmp.S",
        });

    if (t.cpu.arch == .x86_64)
        try srcs.appendSlice(&.{
            "x86_64/is_fpreg.c",
            "x86_64/regname.c",
            "x86_64/Los-linux.c",
            "x86_64/getcontext.S",
            "x86_64/Lapply_reg_state.c",
            "x86_64/Lcreate_addr_space.c",
            "x86_64/Lget_proc_info.c",
            "x86_64/Lget_save_loc.c",
            "x86_64/Lglobal.c",
            "x86_64/Linit.c",
            "x86_64/Linit_local.c",
            "x86_64/Linit_remote.c",
            "x86_64/Lregs.c",
            "x86_64/Lreg_states_iterate.c",
            "x86_64/Lresume.c",
            "x86_64/Lstash_frame.c",
            "x86_64/Lstep.c",
            "x86_64/Ltrace.c",
            // "x86_64/longjmp.S",
            "x86_64/setcontext.S",
            // "x86_64/siglongjmp.S",
        });

    lib.addCSourceFiles(.{
        .root = dep_c.path("src"),
        .files = srcs.items,
        .flags = &.{
            "-fno-sanitize=all",
            "-fexceptions",
            "-Wall",
            "-Wsign-compare",
        },
    });

    lib.installConfigHeader(libunwind_h);
    lib.installConfigHeader(libunwind_common_h);
    lib.installHeadersDirectory(dep_c.path("include"), "", .{
        .include_extensions = &.{
            "libunwind-x86_64.h",
            "libunwind-aarch64.h",
            "libunwind-dynamic.h",
        },
    });

    b.installArtifact(lib);
}
