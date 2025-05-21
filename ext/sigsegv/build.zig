const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const dep_c = b.dependency("sigsegv", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "sigsegv",
        .target = target,
        .optimize = optimize,
    });

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            lib.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            lib.addLibraryPath(macos_sdk.?.path("usr/lib"));
            lib.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    lib.linkLibC();

    lib.root_module.addCMacro("HAVE_CONFIG_H", &[_]u8{});

    const config_h = b.addConfigHeader(.{
        .style = .{
            .autoconf = dep_c.path("config.h.in"),
        },
        .include_path = "config.h",
    }, .{
        .ENABLE_EFAULT = null,
        .HAVE_DLFCN_H = 1,
        .HAVE_EFAULT_SUPPORT = null,
        .HAVE_GETPAGESIZE = 1,
        .HAVE_GETRLIMIT = 1,
        .HAVE_INTTYPES_H = 1,
        .HAVE_MINCORE = 1,
        .HAVE_MMAP_ANON = 1,
        .HAVE_MMAP_ANONYMOUS = 1,
        .HAVE_MMAP_DEVZERO = null,
        .HAVE_MQUERY = null,
        .HAVE_PAGESIZE = null,
        .HAVE_SETRLIMIT = 1,
        .HAVE_SIGALTSTACK = 1,
        .HAVE_STACKVMA = @intFromBool(!(t.os.tag == .linux)),
        .HAVE_STDINT_H = 1,
        .HAVE_STDIO_H = 1,
        .HAVE_STDLIB_H = 1,
        .HAVE_STRINGS_H = 1,
        .HAVE_STRING_H = 1,
        .HAVE_SYSCONF_PAGESIZE = null,
        .HAVE_SYS_SIGNAL_H = 1,
        .HAVE_SYS_STAT_H = 1,
        .HAVE_SYS_TYPES_H = 1,
        .HAVE_UCONTEXT_H = null,
        .HAVE_UINTPTR_T = 1,
        .HAVE_UNISTD_H = 1,
        .HAVE_WORKING_SIGALTSTACK = 1,
        .LT_OBJDIR = null,
        .OLD_CYGWIN_WORKAROUND = null,
        .PACKAGE = null,
        .PACKAGE_BUGREPORT = null,
        .PACKAGE_NAME = null,
        .PACKAGE_STRING = null,
        .PACKAGE_TARNAME = null,
        .PACKAGE_URL = null,
        .PACKAGE_VERSION = null,
        .SIGALTSTACK_SS_REVERSED = null,
        .STACK_DIRECTION = -1,
        .STDC_HEADERS = 1,
        .VERSION = null,
        .stack_t = null,
        .uintptr_t = null,
    });

    if ((t.os.tag == .macos)) {
        config_h.addValues(.{
            .CFG_FAULT = if (t.cpu.arch == .aarch64)
                "fault-macos-arm64.h"
            else
                "fault-macos-i386.h",
            .CFG_HANDLER = "handler-unix.c",
            .CFG_LEAVE = "leave-nop.c",
            .CFG_MACHFAULT = "fault-none.h",
            .CFG_SIGNALS = "signals-macos.h",
            .CFG_STACKVMA = "stackvma-mach.c",
        });
    }

    if ((t.os.tag == .linux)) {
        config_h.addValues(.{
            .CFG_FAULT = if (t.cpu.arch == .aarch64)
                "fault-linux-arm.h"
            else
                "fault-linux-i386.h",
            .CFG_HANDLER = "handler-unix.c",
            .CFG_LEAVE = "leave-nop.c",
            .CFG_MACHFAULT = "fault-none.h",
            .CFG_SIGNALS = "signals.h",
            .CFG_STACKVMA = "stackvma-none.c",
        });
    }

    const sigsegv_h = b.addConfigHeader(.{
        .style = .{
            .cmake = dep_c.path("src/sigsegv.h.in"),
        },
        .include_path = "sigsegv.h",
    }, .{
        .FAULT_CONTEXT = "ucontext_t",
        .FAULT_CONTEXT_INCLUDE = if (t.os.tag == .macos)
            "#include <sys/ucontext.h>"
        else
            "#include <ucontext.h>",
        .HAVE_SIGSEGV_RECOVERY = 1,
        .HAVE_STACK_OVERFLOW_RECOVERY = 1,
    });

    lib.addIncludePath(dep_c.path("src"));
    lib.addConfigHeader(config_h);
    lib.addConfigHeader(sigsegv_h);

    lib.addCSourceFiles(.{
        .root = dep_c.path("src"),
        .files = &.{
            "dispatcher.c",
            "handler.c",
            "leave.c",
            "stackvma.c",
            "version.c",
        },
        .flags = &.{
            "-O2",
            "-fno-sanitize=all",
        },
    });

    lib.installConfigHeader(sigsegv_h);

    b.installArtifact(lib);
}
