const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const dep_c = b.dependency("backtrace", .{
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addStaticLibrary(.{
        .name = "backtrace",
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();

    var config_h: *std.Build.Step.ConfigHeader = undefined;

    if (t.os.tag != .windows) {
        config_h = b.addConfigHeader(.{
            .style = .{
                .autoconf = dep_c.path("config.h.in"),
            },
            .include_path = "config.h",
        }, .{
            .BACKTRACE_ELF_SIZE = 64,
            .BACKTRACE_XCOFF_SIZE = 64,
            .HAVE_ATOMIC_FUNCTIONS = 1,
            .HAVE_CLOCK_GETTIME = 1,
            .HAVE_DECL_GETPAGESIZE = 1,
            .HAVE_DECL_STRNLEN = 1,
            .HAVE_DECL__PGMPTR = 0,
            .HAVE_DLFCN_H = 1,
            .HAVE_DL_ITERATE_PHDR = null,
            .HAVE_FCNTL = 0,
            .HAVE_GETEXECNAME = null,
            .HAVE_GETIPINFO = 1,
            .HAVE_INTTYPES_H = 1,
            .HAVE_KERN_PROC = null,
            .HAVE_KERN_PROC_ARGS = null,
            .HAVE_LIBLZMA = 1,
            .HAVE_LINK_H = null,
            .HAVE_LOADQUERY = null,
            .HAVE_LSTAT = 1,
            .HAVE_MACH_O_DYLD_H = null,
            .HAVE_MEMORY_H = 1,
            .HAVE_READLINK = 1,
            .HAVE_STDINT_H = 1,
            .HAVE_STDLIB_H = 1,
            .HAVE_STRINGS_H = 1,
            .HAVE_STRING_H = 1,
            .HAVE_SYNC_FUNCTIONS = 1,
            .HAVE_SYS_LDR_H = null,
            .HAVE_SYS_LINK_H = null,
            .HAVE_SYS_MMAN_H = 1,
            .HAVE_SYS_STAT_H = 1,
            .HAVE_SYS_TYPES_H = 1,
            .HAVE_TLHELP32_H = null,
            .HAVE_UNISTD_H = 1,
            .HAVE_WINDOWS_H = null,
            .HAVE_ZLIB = 1,
            .HAVE_ZSTD = null,
            .LT_OBJDIR = "",
            .PACKAGE_BUGREPORT = "",
            .PACKAGE_NAME = "",
            .PACKAGE_STRING = "",
            .PACKAGE_TARNAME = "",
            .PACKAGE_URL = "",
            .PACKAGE_VERSION = "",
            .STDC_HEADERS = 1,
            ._ALL_SOURCE = 1,
            ._GNU_SOURCE = 1,
            ._POSIX_PTHREAD_SEMANTICS = 1,
            ._TANDEM_SOURCE = 1,
            .__EXTENSIONS__ = 1,
            ._FILE_OFFSET_BITS = null,
            ._LARGE_FILES = null,
            ._MINIX = null,
            ._POSIX_1_SOURCE = null,
            ._POSIX_SOURCE = null,
        });

        if (t.os.tag.isDarwin()) {
            config_h.addValues(.{
                .HAVE_MACH_O_DYLD_H = 1,
            });
        }
    } else {
        config_h = b.addConfigHeader(.{
            .style = .{
                .autoconf = dep_c.path("config.h.in"),
            },
            .include_path = "config.h",
        }, .{
            .BACKTRACE_ELF_SIZE = 64,
            .BACKTRACE_XCOFF_SIZE = 64,
            .HAVE_ATOMIC_FUNCTIONS = 1,
            .HAVE_CLOCK_GETTIME = 1,
            .HAVE_DECL_GETPAGESIZE = 1,
            .HAVE_DECL_STRNLEN = 1,
            .HAVE_DECL__PGMPTR = 0,
            .HAVE_DLFCN_H = 1,
            .HAVE_DL_ITERATE_PHDR = null,
            .HAVE_FCNTL = null,
            .HAVE_GETEXECNAME = null,
            .HAVE_GETIPINFO = 1,
            .HAVE_INTTYPES_H = 1,
            .HAVE_KERN_PROC = null,
            .HAVE_KERN_PROC_ARGS = null,
            .HAVE_LIBLZMA = 1,
            .HAVE_LINK_H = null,
            .HAVE_LOADQUERY = null,
            .HAVE_LSTAT = null,
            .HAVE_MACH_O_DYLD_H = null,
            .HAVE_MEMORY_H = 1,
            .HAVE_READLINK = null,
            .HAVE_STDINT_H = 1,
            .HAVE_STDLIB_H = 1,
            .HAVE_STRINGS_H = 1,
            .HAVE_STRING_H = 1,
            .HAVE_SYNC_FUNCTIONS = 1,
            .HAVE_SYS_LDR_H = null,
            .HAVE_SYS_LINK_H = null,
            .HAVE_SYS_MMAN_H = null,
            .HAVE_SYS_STAT_H = 1,
            .HAVE_SYS_TYPES_H = 1,
            .HAVE_TLHELP32_H = null,
            .HAVE_UNISTD_H = 1,
            .HAVE_WINDOWS_H = 1,
            .HAVE_ZLIB = 1,
            .HAVE_ZSTD = null,
            .LT_OBJDIR = "",
            .PACKAGE_BUGREPORT = "",
            .PACKAGE_NAME = "",
            .PACKAGE_STRING = "",
            .PACKAGE_TARNAME = "",
            .PACKAGE_URL = "",
            .PACKAGE_VERSION = "",
            .STDC_HEADERS = 1,
            ._ALL_SOURCE = 1,
            ._GNU_SOURCE = 1,
            ._POSIX_PTHREAD_SEMANTICS = 1,
            ._TANDEM_SOURCE = 1,
            .__EXTENSIONS__ = 1,
            ._FILE_OFFSET_BITS = null,
            ._LARGE_FILES = null,
            ._MINIX = null,
            ._POSIX_1_SOURCE = null,
            ._POSIX_SOURCE = null,
        });
    }

    const backtrace_supported_h = b.addConfigHeader(.{
        .style = .{
            .cmake = dep_c.path("backtrace-supported.h.in"),
        },
        .include_path = "backtrace-supported.h",
    }, .{
        .BACKTRACE_SUPPORTED = 1,
        .BACKTRACE_USES_MALLOC = 0,
        .BACKTRACE_SUPPORTS_THREADS = 1,
        .BACKTRACE_SUPPORTS_DATA = 1,
    });

    lib.addConfigHeader(config_h);
    lib.addConfigHeader(backtrace_supported_h);
    lib.addIncludePath(dep_c.path(""));

    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            // libbacktrace_la_SOURCES
            "atomic.c",
            "dwarf.c",
            "fileline.c",
            "posix.c",
            "print.c",
            "sort.c",
            "state.c",
            // BACKTRACE_FILES
            "backtrace.c",
            "simple.c",
            "nounwind.c",
            // FORMAT_FILES
            "elf.c",
            "macho.c",
            "pecoff.c",
            "unknown.c",
            "xcoff.c",
            // VIEW_FILES
            if (t.os.tag == .windows) "read.c" else "mmapio.c",
            // ALLOC_FILES
            if (t.os.tag == .windows) "alloc.c" else "mmap.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("backtrace.h"), "backtrace.h");

    b.installArtifact(lib);
}
