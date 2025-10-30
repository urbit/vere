const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const expat_c = b.dependency("expat", .{
        .target = target,
        .optimize = optimize,
    });
    const expat = b.addStaticLibrary(.{
        .name = "expat",
        .target = target,
        .optimize = optimize,
        .version = .{
            .major = 1,
            .minor = 9,
            .patch = 0,
        },
    });
    expat.linkLibC();

    const expat_cmake_config = b.addConfigHeader(.{
        .style = .{
            .cmake = expat_c.path("expat_config.h.cmake"),
        },
        .include_path = "expat_config.h",
    }, .{
        .PACKAGE_BUGREPORT = "https://github.com/libexpat/libexpat/issues",
        .PACKAGE_NAME = "expat",
        .PACKAGE_STRING = "expat 2.6.0",
        .HAVE_GETPAGESIZE = 1,
        .XML_CONTEXT_BYTES = 1,
        .XML_DEV_URANDOM = 1,
        .XML_GE = 1,
        //
        .BYTEORDER = 1234,
        .PACKAGE_TARNAME = "expat-2.6.0.tar",
        .PACKAGE_VERSION = "2.6.0",
        .OFF_T = "long",
        .SIZE_T = "unsigned",
    });

    expat.addConfigHeader(expat_cmake_config);
    expat.addIncludePath(expat_c.path("lib"));

    expat.addCSourceFiles(.{
        .root = expat_c.path("lib"),
        .files = &.{
            "xmlrole.c",
            // "xmltok_ns.c",
            "xmlparse.c",
            "xmltok.c",
            // "xmltok_impl.c",
        },
    });

    expat.installConfigHeader(expat_cmake_config);
    expat.installHeadersDirectory(expat_c.path("lib"), "", .{
        .include_extensions = &.{
            "expat.h",
            "expat_external.h",
        },
    });

    const dbus_c = b.dependency("dbus", .{
        .target = target,
        .optimize = optimize,
    });
    const dbus = b.addStaticLibrary(.{
        .name = "dbus-1",
        .target = target,
        .optimize = optimize,
        .version = .{
            .major = 3,
            .minor = 38,
            .patch = 0,
        },
    });
    dbus.linkLibC();
    dbus.linkLibrary(expat);

    dbus.root_module.addCMacro("HAVE_ERRNO_H", "");
    dbus.root_module.addCMacro("VERSION", "1.14.8");
    dbus.root_module.addCMacro("SOVERSION", "3.38.0");
    dbus.root_module.addCMacro("DBUS_DAEMON_NAME", "\"dbus\"");
    dbus.root_module.addCMacro("DBUS_COMPILATION", "");
    dbus.root_module.addCMacro("DBUS_VA_COPY", "va_copy");
    dbus.root_module.addCMacro("DBUS_SESSION_BUS_CONNECT_ADDRESS", "\"autolaunch:\"");
    dbus.root_module.addCMacro("DBUS_SYSTEM_BUS_DEFAULT_ADDRESS", "\"unix:tmpdir=/tmp\"");
    dbus.root_module.addCMacro("DBUS_ENABLE_CHECKS", "");
    dbus.root_module.addCMacro("DBUS_ENABLE_ASSERT", "");
    dbus.root_module.addCMacro("HAVE_ALLOCA_H", "");
    dbus.root_module.addCMacro("_GNU_SOURCE", "");
    dbus.root_module.addCMacro("HAVE_SYSLOG_H", "");
    dbus.root_module.addCMacro("HAVE_SOCKLEN_T", "");
    dbus.root_module.addCMacro("HAVE_SYS_RANDOM_H", "");
    dbus.root_module.addCMacro("DBUS_UNIX", "");
    dbus.root_module.addCMacro("HAVE_GETPWNAM_R", "");

    dbus.root_module.addCMacro("ENOMEM", "ERROR_NOT_ENOUGH_MEMORY");
    dbus.root_module.addCMacro("HAVE_STDINT_H", "");
    dbus.root_module.addCMacro("HAVE_SIGNAL_H", "");
    dbus.root_module.addCMacro("HAVE_GETPEEREID", "");

    if (t.os.tag == .linux) {
        dbus.root_module.addCMacro("HAVE_APPARMOR", "");
        dbus.root_module.addCMacro("HAVE_APPARMOR_2_10", "");
        dbus.root_module.addCMacro("HAVE_LIBAUDIT", "");
        dbus.root_module.addCMacro("HAVE_SELINUX", "");
        dbus.root_module.addCMacro("DBUS_HAVE_LINUX_EPOLL", "");
    }

    // dbus.root_module.addCMacro("DBUS_PREFIX", b.fmt("\"{s}\"", .{
    //     b.install_prefix,
    // }));
    // dbus.root_module.addCMacro("DBUS_BINDIR", b.fmt("\"{s}\"", .{
    //     b.getInstallPath(.bin, ""),
    // }));
    // dbus.root_module.addCMacro("DBUS_DATADIR", b.fmt("\"{s}\"", .{
    //     b.getInstallPath(.prefix, "usr/share"),
    // }));
    dbus.root_module.addCMacro("DBUS_MACHINE_UUID_FILE", b.fmt("\"{s}\"", .{
        b.getInstallPath(.prefix, "lib/dbus/machine-id"),
    }));
    // dbus.root_module.addCMacro("DBUS_SYSTEM_CONFIG_FILE", b.fmt("\"{s}\"", .{
    //     b.getInstallPath(.prefix, "usr/share/dbus-1/system.conf"),
    // }));
    // dbus.root_module.addCMacro("DBUS_SESSION_CONFIG_FILE", b.fmt("\"{s}\"", .{
    //     b.getInstallPath(.prefix, "usr/share/dbus-1/session.conf"),
    // }));

    // dbus.root_module.addCMacro("HAVE_CONFIG_H", "");
    // const dbus_config_h = b.addConfigHeader(.{
    //     .style = .{ .autoconf = dbus_c.path("config.h.in") },
    //     .include_path = "config.h",
    // }, .{
    //     .HAVE_ERRNO_H = "",
    //     .VERSION = "1.14.8",
    //     .DBUS_DAEMON_NAME = "\"dbus\"",
    //     .DBUS_VA_COPY = "va_copy",
    //     .DBUS_SESSION_BUS_CONNECT_ADDRESS = "\"autolaunch:\"",
    //     .DBUS_SYSTEM_BUS_DEFAULT_ADDRESS = "\"unix:tmpdir=/tmp\"",
    //     .HAVE_ALLOCA_H = "",
    //     ._GNU_SOURCE = "",
    //     .HAVE_SYSLOG_H = "",
    //     .HAVE_SOCKLEN_T = "",
    //     .HAVE_SYS_RANDOM_H = "",
    //     .DBUS_UNIX = "",
    //     .HAVE_GETPWNAM_R = "",
    //     .HAVE_STDINT_H = "",
    //     .HAVE_SIGNAL_H = "",
    //     .HAVE_GETPEEREID = "",
    //     .@"inline" = null,
    //     .AC_APPLE_UNIVERSAL_BUILD = null,
    //     .DBUS_BINDIR = null,
    //     .DBUS_BUILD_X11 = null,
    //     .DBUS_BUILT_R_DYNAMIC = null,
    //     .DBUS_BUS_ENABLE_INOTIFY = null,
    //     .DBUS_BUS_ENABLE_KQUEUE = null,
    //     .DBUS_CONSOLE_AUTH_DIR = null,
    //     .DBUS_CONSOLE_OWNER_FILE = null,
    //     .DBUS_CYGWIN = null,
    //     .DBUS_DAEMONDIR = null,
    //     .DBUS_DATADIR = null,
    //     .DBUS_DISABLE_ASSERT = null,
    //     .DBUS_DISABLE_CHECKS = null,
    //     .DBUS_ENABLE_EMBEDDED_TESTS = null,
    //     .DBUS_ENABLE_LAUNCHD = null,
    //     .DBUS_ENABLE_MODULAR_TESTS = null,
    //     .DBUS_ENABLE_STATS = null,
    //     .DBUS_ENABLE_VERBOSE_MODE = null,
    //     .DBUS_ENABLE_X11_AUTOLAUNCH = null,
    //     .DBUS_EXEEXT = null,
    //     .DBUS_GCOV_ENABLED = null,
    //     .DBUS_HAVE_LINUX_EPOLL = null,
    //     .DBUS_LIBEXECDIR = null,
    //     .DBUS_PREFIX = null,
    //     .DBUS_SESSION_SOCKET_DIR = null,
    //     .DBUS_SYSTEM_SOCKET = null,
    //     .DBUS_TEST_LAUNCH_HELPER_BINARY = null,
    //     .DBUS_TEST_SOCKET_DIR = null,
    //     .DBUS_TEST_USER = null,
    //     .DBUS_USER = null,
    //     .DBUS_USE_SYNC = null,
    //     .DBUS_WIN = null,
    //     .DBUS_WINCE = null,
    //     .DBUS_WITH_GLIB = null,
    //     .ENABLE_TRADITIONAL_ACTIVATION = null,
    //     .FD_SETSIZE = null,
    //     .GETTEXT_PACKAGE = null,
    //     .GLIB_VERSION_MAX_ALLOWED = null,
    //     .GLIB_VERSION_MIN_REQUIRED = null,
    //     .G_DISABLE_CHECKS = null,
    //     .HAVE_ACCEPT4 = null,
    //     .HAVE_ADT = null,
    //     .HAVE_APPARMOR = null,
    //     .HAVE_APPARMOR_2_10 = null,
    //     .HAVE_BACKTRACE = null,
    //     .HAVE_BYTESWAP_H = null,
    //     .HAVE_CLEARENV = null,
    //     .HAVE_CMSGCRED = null,
    //     .HAVE_CONSOLE_OWNER_FILE = null,
    //     .HAVE_CRT_EXTERNS_H = null,
    //     .HAVE_DDFD = null,
    //     .HAVE_DECL_ENVIRON = null,
    //     .HAVE_DECL_LOG_PERROR = null,
    //     .HAVE_DECL_MSG_NOSIGNAL = null,
    //     .HAVE_DIRENT_H = null,
    //     .HAVE_DIRFD = null,
    //     .HAVE_DLFCN_H = null,
    //     .HAVE_EXECINFO_H = null,
    //     .HAVE_FPATHCONF = null,
    //     .HAVE_GETGROUPLIST = null,
    //     .HAVE_GETPEERUCRED = null,
    //     .HAVE_GETRANDOM = null,
    //     .HAVE_GETRESUID = null,
    //     .HAVE_GETRLIMIT = null,
    //     .HAVE_GIO_UNIX = null,
    //     .HAVE_INOTIFY_INIT1 = null,
    //     .HAVE_INTTYPES_H = null,
    //     .HAVE_ISSETUGID = null,
    //     .HAVE_LIBAUDIT = null,
    //     .HAVE_LIBNSL = null,
    //     .HAVE_LOCALECONV = null,
    //     .HAVE_LOCALE_H = null,
    //     .HAVE_MINIX_CONFIG_H = null,
    //     .HAVE_MONOTONIC_CLOCK = null,
    //     .HAVE_NANOSLEEP = null,
    //     .HAVE_NSGETENVIRON = null,
    //     .HAVE_PIPE2 = null,
    //     .HAVE_POLL = null,
    //     .HAVE_PRCTL = null,
    //     .HAVE_PRLIMIT = null,
    //     .HAVE_RAISE = null,
    //     .HAVE_SELINUX = null,
    //     .HAVE_SETENV = null,
    //     .HAVE_SETLOCALE = null,
    //     .HAVE_SETRESUID = null,
    //     .HAVE_SETRLIMIT = null,
    //     .HAVE_SOCKETPAIR = null,
    //     .HAVE_STDIO_H = null,
    //     .HAVE_STDLIB_H = null,
    //     .HAVE_STRINGS_H = null,
    //     .HAVE_STRING_H = null,
    //     .HAVE_STRTOLL = null,
    //     .HAVE_STRTOULL = null,
    //     .HAVE_SYSTEMD = null,
    //     .HAVE_SYS_INOTIFY_H = null,
    //     .HAVE_SYS_PRCTL_H = null,
    //     .HAVE_SYS_RESOURCE_H = null,
    //     .HAVE_SYS_STAT_H = null,
    //     .HAVE_SYS_TIME_H = null,
    //     .HAVE_SYS_TYPES_H = null,
    //     .HAVE_SYS_UIO_H = null,
    //     .HAVE_UNISTD_H = null,
    //     .HAVE_UNIX_FD_PASSING = null,
    //     .HAVE_UNPCBID = null,
    //     .HAVE_UNSETENV = null,
    //     .HAVE_USLEEP = null,
    //     .HAVE_VISIBILITY = null,
    //     .HAVE_WCHAR_H = null,
    //     .HAVE_WRITEV = null,
    //     .HAVE_WS2TCPIP_H = null,
    //     .HAVE_X11 = null,
    //     .HAVE_XML_SETHASHSALT = null,
    //     .LT_OBJDIR = null,
    //     .NDEBUG = null,
    //     .PACKAGE = null,
    //     .PACKAGE_BUGREPORT = null,
    //     .PACKAGE_NAME = null,
    //     .PACKAGE_STRING = null,
    //     .PACKAGE_TARNAME = null,
    //     .PACKAGE_URL = null,
    //     .PACKAGE_VERSION = null,
    //     .SIZEOF_CHAR = null,
    //     .SIZEOF_INT = null,
    //     .SIZEOF_LONG = null,
    //     .SIZEOF_LONG_LONG = null,
    //     .SIZEOF_SHORT = null,
    //     .SIZEOF_VOID_P = null,
    //     .SIZEOF___INT64 = null,
    //     .STDC_HEADERS = null,
    //     .TEST_LISTEN = null,
    //     .WITH_VALGRIND = null,
    //     .WORDS_BIGENDIAN = null,
    //     ._ALL_SOURCE = null,
    //     ._BSD_SOURCE = null,
    //     ._DARWIN_C_SOURCE = null,
    //     ._FILE_OFFSET_BITS = null,
    //     ._HPUX_ALT_XOPEN_SOCKET_API = null,
    //     ._LARGE_FILES = null,
    //     ._MINIX = null,
    //     ._NETBSD_SOURCE = null,
    //     ._OPENBSD_SOURCE = null,
    //     ._POSIX_1_SOURCE = null,
    //     ._POSIX_C_SOURCE = null,
    //     ._POSIX_PTHREAD_SEMANTICS = null,
    //     ._POSIX_SOURCE = null,
    //     ._TANDEM_SOURCE = null,
    //     ._WIN32_WCE = null,
    //     ._WIN32_WINNT = null,
    //     ._XOPEN_SOURCE = null,
    //     .__EXTENSIONS__ = null,
    //     .__STDC_WANT_IEC_60559_ATTRIBS_EXT__ = null,
    //     .__STDC_WANT_IEC_60559_BFP_EXT__ = null,
    //     .__STDC_WANT_IEC_60559_DFP_EXT__ = null,
    //     .__STDC_WANT_IEC_60559_FUNCS_EXT__ = null,
    //     .__STDC_WANT_IEC_60559_TYPES_EXT__ = null,
    //     .__STDC_WANT_LIB_EXT2__ = null,
    //     .__STDC_WANT_MATH_SPEC_FUNCS__ = null,
    // });

    const dbus_config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{});

    const dbus_arch_deps_h = b.addConfigHeader(
        .{
            .style = .{ .cmake = dbus_c.path("dbus/dbus-arch-deps.h.in") },
            .include_path = "dbus/dbus-arch-deps.h",
        },
        .{
            .DBUS_VERSION = "1.14.10",
            .DBUS_MAJOR_VERSION = "1",
            .DBUS_MINOR_VERSION = "14",
            .DBUS_MICRO_VERSION = "10",
            .DBUS_INT64_TYPE = "long long",
            .DBUS_INT32_TYPE = "int",
            .DBUS_INT16_TYPE = "short",
            .DBUS_SIZEOF_VOID_P = "sizeof (void*)",
            .DBUS_INT64_CONSTANT = "(val##LL)",
            .DBUS_UINT64_CONSTANT = "(val##ULL)",
        },
    );

    dbus.addConfigHeader(dbus_config_h);
    dbus.addConfigHeader(dbus_arch_deps_h);

    dbus.addIncludePath(dbus_c.path(""));
    dbus.addIncludePath(dbus_c.path("dbus"));

    dbus.addCSourceFiles(.{
        .root = dbus_c.path("dbus"),
        .files = &.{
            // DBUS_LIB_SOURCES
            "dbus-address.c",
            "dbus-auth.c",
            "dbus-bus.c",
            "dbus-connection.c",
            "dbus-credentials.c",
            "dbus-errors.c",
            "dbus-keyring.c",
            "dbus-marshal-header.c",
            "dbus-marshal-byteswap.c",
            "dbus-marshal-recursive.c",
            "dbus-marshal-validate.c",
            "dbus-message.c",
            "dbus-misc.c",
            "dbus-nonce.c",
            "dbus-object-tree.c",
            "dbus-pending-call.c",
            "dbus-resources.c",
            "dbus-server.c",
            "dbus-server-socket.c",
            "dbus-server-debug-pipe.c",
            "dbus-sha.c",
            "dbus-signature.c",
            "dbus-syntax.c",
            "dbus-timeout.c",
            "dbus-threads.c",
            "dbus-transport.c",
            "dbus-transport-socket.c",
            "dbus-watch.c",
            // DBUS_SHARED_SOURCES
            "dbus-dataslot.c",
            "dbus-file.c",
            "dbus-hash.c",
            "dbus-internals.c",
            "dbus-list.c",
            "dbus-marshal-basic.c",
            "dbus-memory.c",
            "dbus-mempool.c",
            "dbus-string.c",
            "dbus-sysdeps.c",
            "dbus-pipe.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    // Platform specific sources
    if (target.result.os.tag == .windows) {
        dbus.addCSourceFiles(.{
            .root = dbus_c.path("dbus"),
            .files = &.{
                // LIB
                "dbus-transport-win.c",
                "dbus-server-win.c",
                // SHARED
                "dbus-file-win.c",
                "dbus-init-win.cpp",
                "dbus-sysdeps-win.c",
                "dbus-pipe-win.c",
                "dbus-sysdeps-thread-win.c",
            },
            .flags = &.{
                "-fno-sanitize=all",
            },
        });
    } else {
        dbus.addCSourceFiles(.{
            .root = dbus_c.path("dbus"),
            .files = &.{
                // LIB
                "dbus-uuidgen.c",
                "dbus-transport-unix.c",
                "dbus-server-unix.c",
                // SHARED
                "dbus-file-unix.c",
                "dbus-pipe-unix.c",
                "dbus-sysdeps-unix.c",
                "dbus-sysdeps-pthread.c",
                "dbus-userdb.c",
            },
            .flags = &.{
                "-fno-sanitize=all",
            },
        });
    }

    dbus.installConfigHeader(dbus_arch_deps_h);

    // DBUS CLIENT LIBRARY HEADERS
    dbus.installHeadersDirectory(dbus_c.path("dbus"), "dbus", .{
        .include_extensions = &.{
            // DBUS INCLUDE HEADERS
            "dbus.h",
            "dbus-address.h",
            "dbus-bus.h",
            "dbus-connection.h",
            "dbus-errors.h",
            "dbus-macros.h",
            "dbus-memory.h",
            "dbus-message.h",
            "dbus-misc.h",
            "dbus-pending-call.h",
            "dbus-protocol.h",
            "dbus-server.h",
            "dbus-shared.h",
            "dbus-signature.h",
            "dbus-syntax.h",
            "dbus-threads.h",
            "dbus-types.h",
            "dbus-arch-deps.h",
            // DBUS_LIB_HEADERS
            "dbus-auth.h",
            "dbus-connection-internal.h",
            "dbus-credentials.h",
            "dbus-keyring.h",
            "dbus-marshal-header.h",
            "dbus-marshal-byteswap.h",
            "dbus-marshal-recursive.h",
            "dbus-marshal-validate.h",
            "dbus-message-internal.h",
            "dbus-message-private.h",
            "dbus-misc.h",
            "dbus-object-tree.h",
            "dbus-protocol.h",
            "dbus-resources.h",
            "dbus-server-debug-pipe.h",
            "dbus-server-protected.h",
            "dbus-server-unix.h",
            "dbus-sha.h",
            "dbus-timeout.h",
            "dbus-threads.h",
            "dbus-threads-internal.h",
            "dbus-transport.h",
            "dbus-transport-protected.h",
            "dbus-uuidgen.h",
            "dbus-watch.h",
            // DBUS_SHARED_HEADERS
            "dbus-dataslot.h",
            "dbus-file.h",
            "dbus-hash.h",
            "dbus-internals.h",
            "dbus-list.h",
            "dbus-marshal-basic.h",
            "dbus-mempool.h",
            "dbus-string.h",
            "dbus-string-private.h",
            "dbus-pipe.h",
            "dbus-sysdeps.h",
        },
    });

    // Platform specific headers
    if (target.result.os.tag == .windows) {
        dbus.installHeadersDirectory(dbus_c.path("dbus"), "dbus", .{
            .include_extensions = &.{
                "dbus-transport-win.h",
                "dbus-sockets-win.h",
                "dbus-sysdeps-win.h",
            },
        });
    } else {
        dbus.installHeadersDirectory(dbus_c.path("dbus"), "dbus", .{
            .include_extensions = &.{
                "dbus-transport-unix.h",
                "dbus-server-unix.h",
                "dbus-sysdeps-unix.h",
                "dbus-userdb.h",
            },
        });
    }

    const avahi_c = b.dependency("avahi", .{
        .target = target,
        .optimize = optimize,
    });

    const avahi = b.addStaticLibrary(.{
        .name = "dns-sd",
        .target = target,
        .optimize = optimize,
    });

    avahi.linkLibC();
    avahi.linkLibrary(dbus);

    avahi.root_module.addCMacro("GETTEXT_PACKAGE", "\"avahi\"");

    avahi.root_module.addCMacro("HAVE_DBUS", "");
    avahi.root_module.addCMacro("HAVE_DBUS_BUS_GET_PRIVATE", "0");
    avahi.root_module.addCMacro("HAVE_DBUS_CONNECTION_CLOSE", "0");
    avahi.root_module.addCMacro("HAVE_EXPAT_H", "1");
    avahi.root_module.addCMacro("HAVE_CONFIG_H", "1");

    if (!t.isGnuLibC()) {
        // Non-glibc systems (BSD, macOS, etc...) have strlcpy
        avahi.root_module.addCMacro("HAVE_STRLCPY", "1");
    } else if (t.os.tag == .linux) {
        // If on Linux, check version >= 2.38
        const glibc_version = t.os.version_range.linux.glibc;
        if (glibc_version.order(.{ .major = 2, .minor = 38, .patch = 0 }) != .lt) {
            avahi.root_module.addCMacro("HAVE_STRLCPY", "1");
        }
    }

    const avahi_config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{});

    // avahi.root_module.addCMacro("HAVE_CONFIG_H", "1");
    // const avahi_config_h = b.addConfigHeader(.{
    //     .style = .{ .autoconf = avahi_c.path("config.h.in") },
    //     .include_path = "config.h",
    // }, .{
    //     .AVAHI_AUTOIPD_GROUP = null,
    //     .AVAHI_AUTOIPD_USER = null,
    //     .AVAHI_GROUP = null,
    //     .AVAHI_PRIV_ACCESS_GROUP = null,
    //     .AVAHI_USER = null,
    //     .ENABLE_CHROOT = null,
    //     .ENABLE_NLS = null,
    //     .ENABLE_SSP_CC = null,
    //     .ENABLE_SSP_CXX = null,
    //     .GETTEXT_PACKAGE = "\"avahi\"",
    //     .HAVE_ARPA_INET_H = null,
    //     .HAVE_BSDXML_H = null,
    //     .HAVE_CFLOCALECOPYCURRENT = null,
    //     .HAVE_CFPREFERENCESCOPYAPPVALUE = null,
    //     .HAVE_CHOWN = null,
    //     .HAVE_CHROOT = null,
    //     .HAVE_DBM = null,
    //     .HAVE_DBUS = "1",
    //     .HAVE_DBUS_BUS_GET_PRIVATE = "0",
    //     .HAVE_DBUS_CONNECTION_CLOSE = "0",
    //     .HAVE_DCGETTEXT = null,
    //     .HAVE_DECL_ENVIRON = null,
    //     .HAVE_DLFCN_H = null,
    //     .HAVE_DLOPEN = null,
    //     .HAVE_EXPAT_H = "1",
    //     .HAVE_FCNTL_H = null,
    //     .HAVE_GCC_VISIBILITY = null,
    //     .HAVE_GDBM = null,
    //     .HAVE_GDBM_H = null,
    //     .HAVE_GETHOSTBYNAME = null,
    //     .HAVE_GETHOSTNAME = null,
    //     .HAVE_GETPROGNAME = null,
    //     .HAVE_GETTEXT = null,
    //     .HAVE_GETTIMEOFDAY = null,
    //     .HAVE_ICONV = null,
    //     .HAVE_INOTIFY = null,
    //     .HAVE_INTTYPES_H = null,
    //     .HAVE_KQUEUE = null,
    //     .HAVE_LIMITS_H = null,
    //     .HAVE_MEMCHR = null,
    //     .HAVE_MEMMOVE = null,
    //     .HAVE_MEMORY_H = null,
    //     .HAVE_MEMSET = null,
    //     .HAVE_MKDIR = null,
    //     .HAVE_NDBM_H = null,
    //     .HAVE_NETDB_H = null,
    //     .HAVE_NETINET_IN_H = null,
    //     .HAVE_NETLINK = null,
    //     .HAVE_PF_ROUTE = null,
    //     .HAVE_PTHREAD = null,
    //     .HAVE_PUTENV = null,
    //     .HAVE_SELECT = null,
    //     .HAVE_SETEGID = null,
    //     .HAVE_SETEUID = null,
    //     .HAVE_SETPROCTITLE = null,
    //     .HAVE_SETREGID = null,
    //     .HAVE_SETRESGID = null,
    //     .HAVE_SETRESUID = null,
    //     .HAVE_SETREUID = null,
    //     .HAVE_SOCKET = null,
    //     .HAVE_STAT_EMPTY_STRING_BUG = null,
    //     .HAVE_STDBOOL_H = null,
    //     .HAVE_STDINT_H = null,
    //     .HAVE_STDLIB_H = null,
    //     .HAVE_STRCASECMP = null,
    //     .HAVE_STRCHR = null,
    //     .HAVE_STRCSPN = null,
    //     .HAVE_STRDUP = null,
    //     .HAVE_STRERROR = null,
    //     .HAVE_STRINGS_H = null,
    //     .HAVE_STRING_H = null,
    //     .HAVE_STRLCPY = "1",
    //     .HAVE_STRNCASECMP = null,
    //     .HAVE_STRRCHR = null,
    //     .HAVE_STRSPN = null,
    //     .HAVE_STRSTR = null,
    //     .HAVE_STRUCT_IP_MREQ = null,
    //     .HAVE_STRUCT_IP_MREQN = null,
    //     .HAVE_STRUCT_LIFCONF = null,
    //     .HAVE_SYSLOG_H = null,
    //     .HAVE_SYS_CAPABILITY_H = null,
    //     .HAVE_SYS_FILIO_H = null,
    //     .HAVE_SYS_INOTIFY_H = null,
    //     .HAVE_SYS_IOCTL_H = null,
    //     .HAVE_SYS_PRCTL_H = null,
    //     .HAVE_SYS_SELECT_H = null,
    //     .HAVE_SYS_SOCKET_H = null,
    //     .HAVE_SYS_STAT_H = null,
    //     .HAVE_SYS_SYSCTL_H = null,
    //     .HAVE_SYS_TIME_H = null,
    //     .HAVE_SYS_TYPES_H = null,
    //     .HAVE_SYS_WAIT_H = null,
    //     .HAVE_UNAME = null,
    //     .HAVE_UNISTD_H = null,
    //     .HAVE_VISIBILITY_HIDDEN = null,
    //     .HAVE__BOOL = null,
    //     .LSTAT_FOLLOWS_SLASHED_SYMLINK = null,
    //     .LT_OBJDIR = null,
    //     .PACKAGE = null,
    //     .PACKAGE_BUGREPORT = null,
    //     .PACKAGE_NAME = null,
    //     .PACKAGE_STRING = null,
    //     .PACKAGE_TARNAME = null,
    //     .PACKAGE_URL = null,
    //     .PACKAGE_VERSION = null,
    //     .PTHREAD_CREATE_JOINABLE = null,
    //     .SELECT_TYPE_ARG1 = null,
    //     .SELECT_TYPE_ARG234 = null,
    //     .SELECT_TYPE_ARG5 = null,
    //     .STDC_HEADERS = null,
    //     .TIME_WITH_SYS_TIME = null,
    //     ._ALL_SOURCE = null,
    //     ._GNU_SOURCE = null,
    //     ._POSIX_PTHREAD_SEMANTICS = null,
    //     ._TANDEM_SOURCE = null,
    //     .__EXTENSIONS__ = null,
    //     .VERSION = null,
    //     ._MINIX = null,
    //     ._POSIX_1_SOURCE = null,
    //     ._POSIX_SOURCE = null,
    //     .@"const" = null,
    //     .gid_t = null,
    //     .mode_t = null,
    //     .pid_t = null,
    //     .size_t = null,
    //     .uid_t = null,
    // });

    avahi.addConfigHeader(avahi_config_h);
    avahi.addIncludePath(avahi_c.path(""));

    avahi.addCSourceFiles(.{
        .root = avahi_c.path(""),
        .files = &.{
            // "avahi-autoipd/iface-bsd.c",
            // "avahi-autoipd/iface-linux.c",
            // "avahi-autoipd/main.c",
            "avahi-client/browser.c",
            "avahi-client/check-nss.c",
            "avahi-client/client.c",
            "avahi-client/entrygroup.c",
            "avahi-client/resolver.c",
            "avahi-client/xdg-config.c",
            "avahi-common/address.c",
            "avahi-common/alternative.c",
            "avahi-common/dbus-watch-glue.c",
            "avahi-common/dbus.c",
            "avahi-common/domain.c",
            "avahi-common/error.c",
            "avahi-common/i18n.c",
            "avahi-common/malloc.c",
            "avahi-common/rlist.c",
            "avahi-common/simple-watch.c",
            "avahi-common/strlst.c",
            "avahi-common/thread-watch.c",
            "avahi-common/timeval.c",
            "avahi-common/utf8.c",
            // "avahi-compat-howl/address.c",
            // "avahi-compat-howl/compat.c",
            // "avahi-compat-howl/samples/browse.c",
            // "avahi-compat-howl/samples/publish.c",
            // "avahi-compat-howl/samples/query.c",
            // "avahi-compat-howl/samples/resolve.c",
            // "avahi-compat-howl/text.c",
            // "avahi-compat-howl/unsupported.c",
            // "avahi-compat-howl/warn.c",
            "avahi-compat-libdns_sd/compat.c",
            "avahi-compat-libdns_sd/txt.c",
            "avahi-compat-libdns_sd/unsupported.c",
            "avahi-compat-libdns_sd/warn.c",
            // "avahi-core/addr-util.c",
            // "avahi-core/announce.c",
            // "avahi-core/avahi-reflector.c",
            // "avahi-core/browse-dns-server.c",
            // "avahi-core/browse-domain.c",
            // "avahi-core/browse-service-type.c",
            // "avahi-core/browse-service.c",
            // "avahi-core/browse.c",
            // "avahi-core/cache.c",
            // "avahi-core/dns.c",
            // "avahi-core/domain-util.c",
            // "avahi-core/entry.c",
            // "avahi-core/fdutil.c",
            // "avahi-core/hashmap.c",
            // "avahi-core/iface-linux.c",
            // "avahi-core/iface-none.c",
            // "avahi-core/iface-pfroute.c",
            // "avahi-core/iface.c",
            // "avahi-core/log.c",
            // "avahi-core/multicast-lookup.c",
            // "avahi-core/netlink.c",
            // "avahi-core/prioq.c",
            // "avahi-core/probe-sched.c",
            // "avahi-core/querier.c",
            // "avahi-core/query-sched.c",
            // "avahi-core/resolve-address.c",
            // "avahi-core/resolve-host-name.c",
            // "avahi-core/resolve-service.c",
            // "avahi-core/response-sched.c",
            // "avahi-core/rr.c",
            // "avahi-core/rrlist.c",
            // "avahi-core/server.c",
            // "avahi-core/socket.c",
            // "avahi-core/timeeventq.c",
            // "avahi-core/util.c",
            // "avahi-core/wide-area.c",
            // "avahi-daemon/caps.c",
            // "avahi-daemon/chroot.c",
            // "avahi-daemon/dbus-async-address-resolver.c",
            // "avahi-daemon/dbus-async-host-name-resolver.c",
            // "avahi-daemon/dbus-async-service-resolver.c",
            // "avahi-daemon/dbus-domain-browser.c",
            // "avahi-daemon/dbus-entry-group.c",
            // "avahi-daemon/dbus-protocol.c",
            // "avahi-daemon/dbus-record-browser.c",
            // "avahi-daemon/dbus-service-browser.c",
            // "avahi-daemon/dbus-service-type-browser.c",
            // "avahi-daemon/dbus-sync-address-resolver.c",
            // "avahi-daemon/dbus-sync-host-name-resolver.c",
            // "avahi-daemon/dbus-sync-service-resolver.c",
            // "avahi-daemon/dbus-util.c",
            // "avahi-daemon/ini-file-parser.c",
            // "avahi-daemon/main.c",
            // "avahi-daemon/sd-daemon.c",
            // "avahi-daemon/setproctitle.c",
            // "avahi-daemon/simple-protocol.c",
            // "avahi-daemon/static-hosts.c",
            // "avahi-daemon/static-services.c",
            // "avahi-discover-standalone/main.c",
            // "avahi-dnsconfd/main.c",
            // "avahi-glib/glib-malloc.c",
            // "avahi-glib/glib-watch.c",
            // "avahi-gobject/ga-client.c",
            // "avahi-gobject/ga-entry-group.c",
            // "avahi-gobject/ga-error.c",
            // "avahi-gobject/ga-record-browser.c",
            // "avahi-gobject/ga-service-browser.c",
            // "avahi-gobject/ga-service-resolver.c",
            // "avahi-libevent/libevent-watch.c",
            // "avahi-ui/avahi-ui.c",
            // "avahi-ui/bssh.c",
            // "avahi-utils/avahi-browse.c",
            // "avahi-utils/avahi-publish.c",
            // "avahi-utils/avahi-resolve.c",
            // "avahi-utils/avahi-set-host-name.c",
            // "avahi-utils/sigint.c",
            // "avahi-utils/stdb.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    avahi.installHeader(avahi_c.path("avahi-compat-libdns_sd/dns_sd.h"), "dns_sd.h");

    b.installArtifact(avahi);
}
