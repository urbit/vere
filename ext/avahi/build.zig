const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

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

    // const expat_cmake_config = b.addConfigHeader(
    //     .{ .style = .{ .cmake = expat_c.path("expat_config.h.in") } },
    //     .{}
    // );
    // expat.addConfigHeader(expat_cmake_config);
    // const expat_install_cmake_config = b.addInstallFile(
    //     expat_cmake_config.getOutput(),
    //     "expat_config.h"
    // );
    // b.getInstallStep().dependOn(&expat_install_cmake_config.step);

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

    // b.installArtifact(expat);

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
    dbus.root_module.addCMacro("VERSION", "1.14.10");
    dbus.root_module.addCMacro("SOVERSION", "3.38.0");
    dbus.root_module.addCMacro("DBUS_DAEMON_NAME", "\"dbus\"");
    dbus.root_module.addCMacro("DBUS_COMPILATION", "");
    dbus.root_module.addCMacro("DBUS_VA_COPY", "va_copy");
    dbus.root_module.addCMacro("DBUS_SESSION_BUS_CONNECT_ADDRESS",
                               "\"autolaunch:\"");
    dbus.root_module.addCMacro("DBUS_SYSTEM_BUS_DEFAULT_ADDRESS",
                               "\"unix:tmpdir=/tmp\"");
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

    // dbus.root_module.addCMacro(
    //   "DBUS_PREFIX",
    //   b.fmt("\"{s}\"", .{b.install_prefix})
    // );
    // dbus.root_module.addCMacro(
    //   "DBUS_BINDIR",
    //   b.fmt("\"{s}\"", .{b.getInstallPath(.bin, "")})
    // );
    // dbus.root_module.addCMacro(
    //   "DBUS_DATADIR",
    //   b.fmt("\"{s}\"", .{b.getInstallPath(.prefix, "usr/share")})
    // );
    dbus.root_module.addCMacro(
        "DBUS_MACHINE_UUID_FILE",
        b.fmt("\"{s}\"", .{b.getInstallPath(.prefix, "lib/dbus/machine-id")})
    );
    // dbus.root_module.addCMacro(
    //   "DBUS_SYSTEM_CONFIG_FILE",
    //   b.fmt("\"{s}\"", .{b.getInstallPath(.prefix, "usr/share/dbus-1/system.conf")})
    // );
    // dbus.root_module.addCMacro(
    //   "DBUS_SESSION_CONFIG_FILE",
    //   b.fmt("\"{s}\"", .{b.getInstallPath(.prefix, "usr/share/dbus-1/session.conf")})
    // );

    // const dbus_config_header = b.addWriteFile("config.h", blk: {
    //   var output = std.ArrayList(u8).init(b.allocator);
    //   defer output.deinit();

    //   try output.appendSlice(
    //     \\#pragma once
    //     \\
    //     \\#define HAVE_ERRNO_H
    //     \\#include <stdarg.h>
    //     \\#include <stdint.h>
    //     \\#include <unistd.h>
    //     \\
    //     \\#define VERSION "1.14.10"
    //     \\#define SOVERSION "3.38.0"
    //     \\#define DBUS_DAEMON_NAME "\"dbus\""
    //     \\#define DBUS_COMPILATION
    //     \\#define DBUS_VA_COPY va_copy
    //     \\#define DBUS_SESSION_BUS_CONNECT_ADDRESS "\"autolaunch:\""
    //     \\#define DBUS_SYSTEM_BUS_DEFAULT_ADDRESS "\"unix:tmpdir=/tmp\""
    //     \\#define DBUS_ENABLE_CHECKS
    //     \\#define DBUS_ENABLE_ASSERT
    //     \\#define HAVE_ALLOCA_H
    //     \\
    //   );

    //   if (target.result.abi == .gnu) {
    //     try output.appendSlice(
    //       \\#define __USE_GNU
    //       \\
    //     );
    //   }

    //   if (target.result.os.tag == .windows) {
    //     try output.appendSlice(
    //       \\#define DBUS_WIN
    //       \\
    //     );
    //   } else {
    //     try output.appendSlice(b.fmt(
    //       \\#define _GNU_SOURCE
    //       \\#define HAVE_SYSLOG_H
    //       \\#define HAVE_SOCKLEN_T
    //       \\#define HAVE_SYS_RANDOM_H
    //       \\
    //       \\#include <signal.h>
    //       \\#include <sys/types.h>
    //       \\
    //       \\#define DBUS_UNIX
    //       \\#define HAVE_GETPWNAM_R
    //       \\#define DBUS_PREFIX "{s}"
    //       \\#define DBUS_BINDIR "{s}"
    //       \\#define DBUS_DATADIR "{s}"
    //       \\#define DBUS_MACHINE_UUID_FILE "{s}"
    //       \\#define DBUS_SYSTEM_CONFIG_FILE "{s}"
    //       \\#define DBUS_SESSION_CONFIG_FILE "{s}"
    //       \\
    //       , .{
    //         b.install_prefix,
    //         b.getInstallPath(.bin, ""),
    //         b.getInstallPath(.prefix, "usr/share"),
    //         b.getInstallPath(.prefix, "var/lib/dbus/machine-id"),
    //         b.getInstallPath(.prefix, "usr/share/dbus-1/system.conf"),
    //         b.getInstallPath(.prefix, "usr/share/dbus-1/session.conf"),
    //     }));
    //   }

    //   if (target.result.os.tag == .linux) {
    //     try output.appendSlice(
    //       \\#define HAVE_APPARMOR
    //       \\#define HAVE_APPARMOR_2_10
    //       \\#define HAVE_LIBAUDIT
    //       \\#define HAVE_SELINUX
    //       \\#define DBUS_HAVE_LINUX_EPOLL
    //       \\
    //     );
    //   }

    //   break :blk try output.toOwnedSlice();
    // });

    // const dbus_cmake_config = b.addConfigHeader(.{
    //     .style = .{ .cmake = dbus_c.path("cmake/config.h.cmake") },
    //     .include_path = "config.h"
    //   }, .{
    //   // .HAVE_ERRNO_H = "",
    //   // .VERSION = "1.14.10",
    //   // .SOVERSION = "3.38.0",
    //   // .DBUS_DAEMON_NAME = "\"dbus\"",
    //   // .DBUS_COMPILATION = 1,
    //   // .DBUS_VA_COPY = "va_copy",
    //   // .DBUS_SESSION_BUS_CONNECT_ADDRESS = "autolaunch:",
    //   // .DBUS_SYSTEM_BUS_DEFAULT_ADDRESS = "unix:tmpdir=/tmp",
    //   // // .DBUS_ENABLE_CHECKS = "",
    //   // // .DBUS_ENABLE_ASSERT = "",
    //   // .HAVE_ALLOCA_H = "",
    //   // ._GNU_SOURCE = "",
    //   // .HAVE_SYSLOG_H = "",
    //   // .HAVE_SOCKLEN_T = "",
    //   // .HAVE_SYS_RANDOM_H = "",
    //   // .DBUS_UNIX = "",
    //   // .HAVE_GETPWNAM_R = "",
    //   // //
    //   // // EXTRA
    //   // //
    //   // .ENOMEM = "ERROR_NOT_ENOUGH_MEMORY",
    //   // ---------------------------------------------------------------
    //   // CMAKE MISSING VALUE
    //   //
    //   .AUTOPACKAGE_CONFIG_H_TEMPLATE = "",
    //   .DBUS_CONSOLE_AUTH_DIR = 0,
    //   .DBUS_DATADIR = 0,
    //   .DBUS_BINDIR = 0,
    //   .DBUS_PREFIX = 0,
    //   .DBUS_SYSTEM_CONFIG_FILE = 0,
    //   .DBUS_SESSION_CONFIG_FILE = 0,
    //   .DBUS_SESSION_SOCKET_DIR = 0,
    //   .DBUS_DAEMON_NAME = "\"dbus\"",
    //   .DBUS_SYSTEM_BUS_DEFAULT_ADDRESS = "unix:tmpdir=/tmp",
    //   .DBUS_SESSION_BUS_CONNECT_ADDRESS = "autolaunch:",
    //   .DBUS_MACHINE_UUID_FILE = 0,
    //   .DBUS_DAEMONDIR = 0,
    //   .DBUS_RUNSTATEDIR = 0,
    //   .TEST_LISTEN = 0,
    //   .EXEEXT = 0,
    //   .DBUS_CONSOLE_OWNER_FILE = 0,
    //   .DBUS_VA_COPY = "va_copy",
    //   .GLIB_VERSION_MIN_REQUIRED = 0,
    //   .GLIB_VERSION_MAX_ALLOWED = 0,
    //   .FD_SETSIZE = 0,
    //   .DBUS_USER = 0,
    //   .DBUS_TEST_USER = 0,
    //   .DBUS_TEST_EXEC = 0,
    // });
    // dbus.addConfigHeader(dbus_cmake_config);
    // const dbus_install_cmake_config = b.addInstallFile(
    //     dbus_cmake_config.getOutput(),
    //     "dbus/config.h"
    // );
    // b.getInstallStep().dependOn(&dbus_install_cmake_config.step);

    const dbus_config_h = b.addConfigHeader(.{
        .style = .blank, .include_path = "config.h"
        }, .{});

    const dbus_arch_deps_h = b.addConfigHeader(.{
        .style = .{ .cmake = dbus_c.path("dbus/dbus-arch-deps.h.in") },
        .include_path = "dbus/dbus-arch-deps.h"
        }, .{
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
        }
                                               );
    // const dbus_install_arch_deps_h = b.addInstallFile(
    //   dbus_arch_deps_h.getOutput(),
    //   "include/dbus/dbus-arch-deps.h"
    // );
    // b.getInstallStep().dependOn(&dbus_install_arch_deps_h.step);

    dbus.addConfigHeader(dbus_config_h);
    dbus.addConfigHeader(dbus_arch_deps_h);

    // const dbus_install_cmake_arch_deps = b.addInstallFile(
    //     dbus_cmake_config.getOutput(),
    //     "dbus/dbus-arch-deps.h"
    // );
    // b.getInstallStep().dependOn(&dbus_install_cmake_arch_deps.step);

    dbus.addIncludePath(dbus_c.path(""));
    dbus.addIncludePath(dbus_c.path("dbus"));
    // dbus.addIncludePath(dbus_config_header.getDirectory());

    // var dbus_prefix_buf: [4096]u8 = undefined;
    // var dbus_bindir_buf: [4096]u8 = undefined;
    // var dbus_datadir_buf: [4096]u8 = undefined;
    // var dbus_machine_uuid_file_buf: [4096]u8 = undefined;
    // var dbus_system_config_file_buf: [4096]u8 = undefined;
    // var dbus_session_config_file_buf: [4096]u8 = undefined;

    // const dbus_prefix = try std.fmt.bufPrint(
    //   &dbus_prefix_buf,
    //   "-DDBUS_PREFIX={s}",
    //   .{b.install_prefix}
    // );
    // const dbus_bindir = try std.fmt.bufPrint(
    //   &dbus_bindir_buf,
    //   "-DDBUS_BINDIR={s}",
    //   b.getInstallPath(.bin, "")
    // );
    // const dbus_datadir = try std.fmt.bufPrint(
    //   &dbus_datadir_buf,
    //   "-DDBUS_DATADIR={s}",
    //   b.getInstallPath(.prefix, "usr/share")
    // );
    // const dbus_machine_uuid_file = try std.fmt.bufPrint(
    //   &dbus_machine_uuid_file_buf,
    //   "-DDBUS_MACHINE_UUID_FILE={s}",
    //   b.getInstallPath(.prefix, "var/lib/dbus/machine-id")
    // );
    // const dbus_system_config_file = try std.fmt.bufPrint(
    //   &dbus_system_config_file_buf,
    //   "-DDBUS_SYSTEM_CONFIG_FILE={s}",
    //   b.getInstallPath(.prefix, "usr/share/dbus-1/system.conf")
    // );
    // const dbus_session_config_file = try std.fmt.bufPrint(
    //   &dbus_session_config_file_buf,
    //   "-DDBUS_SESSION_CONFIG_FILE={s}",
    //   b.getInstallPath(.prefix, "usr/share/dbus-1/session.conf")
    // );
    // .flags = &.{
    //   dbus_prefix,
    //   dbus_bindir,
    //   dbus_datadir,
    //   dbus_machine_uuid_file,
    //   dbus_system_config_file,
    //   dbus_session_config_file,
    // },

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
    if ( target.result.os.tag == .windows ) {
        dbus.installHeadersDirectory(dbus_c.path("dbus"), "dbus", .{
            .include_extensions = &.{
                "dbus-transport-win.h",
                "dbus-sockets-win.h",
                "dbus-sysdeps-win.h",
            }
        });
    }
    else {
        dbus.installHeadersDirectory(dbus_c.path("dbus"), "dbus", .{
            .include_extensions = &.{
                "dbus-transport-unix.h",
                "dbus-server-unix.h",
                "dbus-sysdeps-unix.h",
                "dbus-userdb.h",
            }
        });
    }

    // b.installArtifact(dbus);

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

    // avahi.root_module.addCMacro("__GNUC__", "4");
    // avahi.root_module.addCMacro("__GNUC_MINOR__", "3");
    avahi.root_module.addCMacro("GETTEXT_PACKAGE", "\"avahi\"");

    avahi.root_module.addCMacro("HAVE_DBUS", "");
    avahi.root_module.addCMacro("HAVE_DBUS_BUS_GET_PRIVATE", "0");
    avahi.root_module.addCMacro("HAVE_DBUS_CONNECTION_CLOSE", "0");
    avahi.root_module.addCMacro("HAVE_EXPAT_H", "1");
    avahi.root_module.addCMacro("HAVE_CONFIG_H", "1");
    avahi.root_module.addCMacro("HAVE_STRLCPY", "1");

    const avahi_config_h = b.addConfigHeader(
        .{.style = .blank, .include_path = "config.h"},
        .{}
    );

    // const avahi_config_h = b.addConfigHeader(.{
    //   .style = .{ .autoconf = avahi_c.path("config.h.in") },
    //   .include_path = "config.h"
    //   },
    //   .{
    //     .GETTEXT_PACKAGE = "\"UTF-8\"",
    //   }
    // );

    avahi.addConfigHeader(avahi_config_h);

    // avahi-dnsconfd
    // avahi-autoipd
    // avahi-utils
    // avahi-daemon
    // avahi-libevent
    // avahi-compat-howl
    // avahi-gobject
    // avahi-common
    // avahi-client
    // avahi-discover-standalone
    // avahi-sharp
    // avahi-python
    // avahi-python/avahi-discover
    // avahi-ui-sharp
    // avahi-ui
    // avahi-glib
    // avahi-core
    // avahi-compat-libdns_sd
    // avahi-qt

    // avahi.addIncludePath(b.getInstallPath(.header, ""));
    avahi.addIncludePath(avahi_c.path(""));
    // avahi.addIncludePath(avahi_c.path("avahi-dnsconfd"));
    // avahi.addIncludePath(avahi_c.path("avahi-autoipd"));
    // avahi.addIncludePath(avahi_c.path("avahi-utils"));
    // avahi.addIncludePath(avahi_c.path("avahi-daemon"));
    // avahi.addIncludePath(avahi_c.path("avahi-libevent"));
    // avahi.addIncludePath(avahi_c.path("avahi-compat-howl"));
    // avahi.addIncludePath(avahi_c.path("avahi-gobject"));
    // avahi.addIncludePath(avahi_c.path("avahi-common"));
    // avahi.addIncludePath(avahi_c.path("avahi-client"));
    // avahi.addIncludePath(avahi_c.path("avahi-discover-standalone"));
    // avahi.addIncludePath(avahi_c.path("avahi-sharp"));
    // avahi.addIncludePath(avahi_c.path("avahi-python"));
    // avahi.addIncludePath(avahi_c.path("avahi-python/avahi-discover"));
    // avahi.addIncludePath(avahi_c.path("avahi-ui-sharp"));
    // avahi.addIncludePath(avahi_c.path("avahi-ui"));
    // avahi.addIncludePath(avahi_c.path("avahi-glib"));
    // avahi.addIncludePath(avahi_c.path("avahi-core"));
    avahi.addIncludePath(avahi_c.path("avahi-compat-libdns_sd"));
    // avahi.addIncludePath(avahi_c.path("avahi-qt"));

    avahi.addCSourceFiles(.{
        .root = avahi_c.path(""),
        .files = &.{
            // "avahi-dnsconfd/main.c",
            // "avahi-autoipd/iface-linux.c",
            // "avahi-autoipd/main.c",
            // "avahi-autoipd/iface-bsd.c",
            // "avahi-utils/avahi-set-host-name.c",
            // "avahi-utils/avahi-resolve.c",
            // "avahi-utils/avahi-publish.c",
            // "avahi-utils/sigint.c",
            // "avahi-utils/avahi-browse.c",
            // "avahi-utils/stdb.c",
            // "avahi-daemon/dbus-sync-address-resolver.c",
            // "avahi-daemon/dbus-util.c",
            // "avahi-daemon/dbus-service-type-browser.c",
            // "avahi-daemon/dbus-async-host-name-resolver.c",
            // "avahi-daemon/dbus-sync-service-resolver.c",
            // "avahi-daemon/setproctitle.c",
            // "avahi-daemon/ini-file-parser-test.c",
            // "avahi-daemon/dbus-domain-browser.c",
            // "avahi-daemon/simple-protocol.c",
            // "avahi-daemon/dbus-protocol.c",
            // "avahi-daemon/static-services.c",
            // "avahi-daemon/dbus-entry-group.c",
            // "avahi-daemon/caps.c",
            // "avahi-daemon/dbus-record-browser.c",
            // "avahi-daemon/chroot.c",
            // "avahi-daemon/static-hosts.c",
            // "avahi-daemon/main.c",
            // "avahi-daemon/sd-daemon.c",
            // "avahi-daemon/ini-file-parser.c",
            // "avahi-daemon/dbus-async-address-resolver.c",
            // "avahi-daemon/dbus-service-browser.c",
            // "avahi-daemon/dbus-async-service-resolver.c",
            // "avahi-daemon/dbus-sync-host-name-resolver.c",
            // "avahi-libevent/libevent-watch-test.c",
            // "avahi-libevent/libevent-watch.c",
            // "avahi-compat-howl/address-test.c",
            // "avahi-compat-howl/compat.c",
            // "avahi-compat-howl/browse-domain-test.c",
            // "avahi-compat-howl/text-test.c",
            // "avahi-compat-howl/samples/publish.c",
            // "avahi-compat-howl/samples/resolve.c",
            // "avahi-compat-howl/samples/browse.c",
            // "avahi-compat-howl/samples/query.c",
            // "avahi-compat-howl/warn.c",
            // "avahi-compat-howl/unsupported.c",
            // "avahi-compat-howl/text.c",
            // "avahi-compat-howl/address.c",
            // "avahi-gobject/ga-entry-group.c",
            // "avahi-gobject/ga-error.c",
            // "avahi-gobject/ga-client.c",
            // "avahi-gobject/ga-record-browser.c",
            // "avahi-gobject/ga-service-resolver.c",
            // "avahi-gobject/ga-service-browser.c",
            // "avahi-common/timeval.c",
            // "avahi-common/watch-test.c",
            // "avahi-common/alternative-test.c",
            // "avahi-common/dbus-watch-glue.c",
            // "avahi-common/utf8.c",
            // "avahi-common/rlist.c",
            // "avahi-common/alternative.c",
            // "avahi-common/strlst.c",
            // "avahi-common/error.c",
            // "avahi-common/thread-watch.c",
            // "avahi-common/malloc.c",
            // "avahi-common/utf8-test.c",
            // "avahi-common/strlst-test.c",
            // "avahi-common/address.c",
            // "avahi-common/dbus.c",
            // "avahi-common/domain-test.c",
            // "avahi-common/timeval-test.c",
            // "avahi-common/i18n.c",
            // "avahi-common/simple-watch.c",
            // "avahi-common/domain.c",
            // "avahi-client/client-test.c",
            // "avahi-client/browser.c",
            // "avahi-client/xdg-config-test.c",
            // "avahi-client/resolver.c",
            // "avahi-client/srv-test.c",
            // "avahi-client/check-nss.c",
            // "avahi-client/client.c",
            // "avahi-client/check-nss-test.c",
            // "avahi-client/entrygroup.c",
            // "avahi-client/xdg-config.c",
            // "avahi-client/rr-test.c",
            // "avahi-discover-standalone/main.c",
            // "avahi-ui/bssh.c",
            // "avahi-ui/avahi-ui.c",
            // "avahi-glib/glib-malloc.c",
            // "avahi-glib/glib-watch.c",
            // "avahi-glib/glib-watch-test.c",
            // "avahi-core/hashmap-test.c",
            // "avahi-core/log.c",
            // "avahi-core/resolve-address.c",
            // "avahi-core/query-sched.c",
            // "avahi-core/dns-spin-test.c",
            // "avahi-core/iface.c",
            // "avahi-core/dns-test.c",
            // "avahi-core/rr.c",
            // "avahi-core/multicast-lookup.c",
            // "avahi-core/util.c",
            // "avahi-core/iface-pfroute.c",
            // "avahi-core/timeeventq.c",
            // "avahi-core/announce.c",
            // "avahi-core/domain-util.c",
            // "avahi-core/wide-area.c",
            // "avahi-core/dns.c",
            // "avahi-core/probe-sched.c",
            // "avahi-core/avahi-reflector.c",
            // "avahi-core/iface-linux.c",
            // "avahi-core/timeeventq-test.c",
            // "avahi-core/browse.c",
            // "avahi-core/resolve-host-name.c",
            // "avahi-core/response-sched.c",
            // "avahi-core/browse-service.c",
            // "avahi-core/rrlist.c",
            // "avahi-core/browse-service-type.c",
            // "avahi-core/querier-test.c",
            // "avahi-core/socket.c",
            // "avahi-core/avahi-test.c",
            // "avahi-core/resolve-service.c",
            // "avahi-core/server.c",
            // "avahi-core/conformance-test.c",
            // "avahi-core/prioq-test.c",
            // "avahi-core/iface-none.c",
            // "avahi-core/browse-dns-server.c",
            // "avahi-core/cache.c",
            // "avahi-core/prioq.c",
            // "avahi-core/browse-domain.c",
            // "avahi-core/update-test.c",
            // "avahi-core/netlink.c",
            // "avahi-core/entry.c",
            // "avahi-core/addr-util.c",
            // "avahi-core/hashmap.c",
            // "avahi-core/querier.c",
            // "avahi-core/fdutil.c",
            // "avahi-compat-libdns_sd/null-test.c",
            "avahi-compat-libdns_sd/compat.c",
            // "avahi-compat-libdns_sd/txt-test.c",
            "avahi-compat-libdns_sd/txt.c",
            "avahi-compat-libdns_sd/warn.c",
            "avahi-compat-libdns_sd/unsupported.c",
        },
    });

    avahi.installHeader(
        avahi_c.path("avahi-compat-libdns_sd/dns_sd.h"),
        "dns_sd.h"
    );

    b.installArtifact(avahi);
}
