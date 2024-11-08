const std = @import("std");

const VERSION = "3.2";

const targets: []const std.Target.Query = &.{
    .{ .cpu_arch = .aarch64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .x86_64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .musl },
};

const BuildCfg = struct {
    version: []const u8,
    pace: []const u8,
    binary_name: []const u8,
    flags: []const []const u8 = &.{},
    include_test_steps: bool = true,
    cpu_dbg: bool = false,
    mem_dbg: bool = false,
    c3dbg: bool = false,
    snapshot_validation: bool = false,
};

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{ .whitelist = targets });
    var optimize = b.standardOptimizeOption(.{});

    //
    // Additional Project-Specific Options
    //

    const all = b.option(bool, "all", "Build all targets") orelse false;

    const release = b.option(bool, "release", "Build for release") orelse false;
    if (release) optimize = .ReleaseFast;

    const Pace = enum { once, live, soon, edge };
    const pace = @tagName(b.option(
        Pace,
        "pace",
        "Release train (Default: once)",
    ) orelse if (release) Pace.live else Pace.once);

    const copts: []const []const u8 = b.option(
        []const []const u8,
        "copt",
        "Provide additional compiler flags",
    ) orelse &.{};

    const cpu_dbg = b.option(
        bool,
        "cpu-dbg",
        "Enable cpu debug mode (-DU3_CPU_DEBUG)",
    ) orelse false;

    const mem_dbg = b.option(
        bool,
        "mem-dbg",
        "Enable memory debug mode (-DU3_MEMORY_DEBUG)",
    ) orelse false;

    const c3dbg = b.option(
        bool,
        "c3dbg",
        "Enable debug assertions (-DC3DBG)",
    ) orelse false;

    const snapshot_validation = b.option(
        bool,
        "snapshot-validation",
        "Enable snapshot validation (-DU3_SNAPSHOT_VALIDATION)",
    ) orelse false;

    const binary_name = b.option(
        []const u8,
        "binary-name",
        "Binary name (Default: urbit)",
    ) orelse "urbit";

    // Parse short git rev
    var file = try std.fs.cwd().openFile(".git/logs/HEAD", .{});
    defer file.close();
    var buf_reader = std.io.bufferedReader(file.reader());
    var in_stream = buf_reader.reader();
    var buf: [1024]u8 = undefined;
    var last_line: [1024]u8 = undefined;
    while (try in_stream.readUntilDelimiterOrEof(&buf, '\n')) |line| {
        if (line.len > 0)
            last_line = buf;
    }
    const git_rev = buf[41..51];

    // Binary version
    const version = if (!release)
        VERSION ++ "-" ++ git_rev
    else
        VERSION;

    //
    // Build
    //

    const build_cfg: BuildCfg = .{
        .version = version,
        .pace = pace,
        .flags = copts,
        .binary_name = binary_name,
        .cpu_dbg = cpu_dbg,
        .mem_dbg = mem_dbg,
        .c3dbg = c3dbg,
        .snapshot_validation = snapshot_validation,
        .include_test_steps = !all,
    };

    if (all) {
        for (targets) |t| {
            try buildBinary(b, b.resolveTargetQuery(t), optimize, build_cfg);
        }
    } else {
        const t = target.result;
        try buildBinary(
            b,
            if (t.os.tag == .linux and target.query.isNative())
                b.resolveTargetQuery(.{ .abi = .musl })
            else
                target,
            optimize,
            build_cfg,
        );
    }
}

fn buildBinary(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cfg: BuildCfg,
) !void {
    const t = target.result;

    //
    // Global C Opts
    // TODO: Propagate these to all 3rd party dependencies
    //

    var global_flags = std.ArrayList([]const u8).init(b.allocator);
    defer global_flags.deinit();

    try global_flags.appendSlice(cfg.flags);
    try global_flags.appendSlice(&.{
        "-fno-sanitize=all",
        "-g",
        "-Wall",
        "-Werror",
    });

    //
    //  C Opts for Urbit PKGs And Binary
    //

    var urbit_flags = std.ArrayList([]const u8).init(b.allocator);
    defer urbit_flags.deinit();

    try urbit_flags.appendSlice(global_flags.items);
    try urbit_flags.appendSlice(&.{
        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fms-extensions",
        "-DU3_GUARD_PAGE", // pkg_noun
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

    if (cfg.cpu_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_CPU_DEBUG"});

    if (cfg.mem_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_MEMORY_DEBUG"});

    if (cfg.c3dbg)
        try urbit_flags.appendSlice(&.{"-DC3DBG"});

    if (cfg.snapshot_validation)
        try urbit_flags.appendSlice(&.{"-DU3_SNAPSHOT_VALIDATION"});

    if (t.cpu.arch == .aarch64) {
        try urbit_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .macos) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
            "-DENT_GETENTROPY_SYSRANDOM", // pkg_ent
        });
    } else {
        try urbit_flags.appendSlice(&.{
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

    if (t.os.tag == .linux) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
        });
    }

    //
    // Dependencies
    //

    const copts: []const []const u8 = urbit_flags.items;

    const pkg_c3 = b.dependency("pkg_c3", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ent = b.dependency("pkg_ent", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ur = b.dependency("pkg_ur", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_vere = b.dependency("pkg_vere", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
        .pace = cfg.pace,
        .version = cfg.version,
    });

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const natpmp = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const sigsegv = b.dependency("sigsegv", .{
        .target = target,
        .optimize = optimize,
    });

    const urcrypt = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const whereami = b.dependency("whereami", .{
        .target = target,
        .optimize = optimize,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
    });

    //
    // Build Artifact
    //

    const urbit = b.addExecutable(.{
        .name = cfg.binary_name,
        .target = target,
        .optimize = optimize,
    });
    urbit.stack_size = 0;

    const target_query: std.Target.Query = .{
        .cpu_arch = t.cpu.arch,
        .os_tag = t.os.tag,
        .abi = t.abi,
        .os_version_min = .none,
        .os_version_max = .none,
    };
    const target_output = b.addInstallArtifact(urbit, .{
        .dest_dir = .{
            .override = .{
                .custom = try target_query.zigTriple(b.allocator),
            },
        },
    });
    b.getInstallStep().dependOn(&target_output.step);

    if (target.result.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            urbit.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            urbit.addLibraryPath(macos_sdk.?.path("usr/lib"));
            urbit.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    urbit.linkLibC();

    urbit.linkLibrary(pkg_vere.artifact("vere"));
    urbit.linkLibrary(pkg_noun.artifact("noun"));
    urbit.linkLibrary(pkg_c3.artifact("c3"));
    urbit.linkLibrary(pkg_ur.artifact("ur"));

    urbit.linkLibrary(gmp.artifact("gmp"));
    urbit.linkLibrary(h2o.artifact("h2o"));
    urbit.linkLibrary(curl.artifact("curl"));
    urbit.linkLibrary(libuv.artifact("libuv"));
    urbit.linkLibrary(lmdb.artifact("lmdb"));
    urbit.linkLibrary(openssl.artifact("ssl"));
    urbit.linkLibrary(sigsegv.artifact("sigsegv"));
    urbit.linkLibrary(urcrypt.artifact("urcrypt"));
    urbit.linkLibrary(whereami.artifact("whereami"));

    urbit.addCSourceFiles(.{
        .root = b.path("pkg/vere"),
        .files = &.{
            "main.c",
        },
        .flags = urbit_flags.items,
    });

    //
    // Tests
    //

    if (cfg.include_test_steps) {
        // pkg_ur
        addTest(
            b,
            target,
            optimize,
            "ur-test",
            "pkg/ur/tests.c",
            &.{pkg_ur.artifact("ur")},
            urbit_flags.items,
        );

        // pkg_ent
        addTest(
            b,
            target,
            optimize,
            "ent-test",
            "pkg/ent/tests.c",
            &.{pkg_ent.artifact("ent")},
            urbit_flags.items,
        );

        // pkg_noun
        addTest(
            b,
            target,
            optimize,
            "hashtable-test",
            "pkg/noun/hashtable_tests.c",
            &.{
                pkg_noun.artifact("noun"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "jets-test",
            "pkg/noun/jets_tests.c",
            &.{
                pkg_noun.artifact("noun"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "nock-test",
            "pkg/noun/nock_tests.c",
            &.{
                pkg_noun.artifact("noun"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "retrieve-test",
            "pkg/noun/retrieve_tests.c",
            &.{
                pkg_noun.artifact("noun"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "serial-test",
            "pkg/noun/serial_tests.c",
            &.{
                pkg_noun.artifact("noun"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
            },
            urbit_flags.items,
        );

        // vere
        addTest(
            b,
            target,
            optimize,
            "ames-test",
            "pkg/vere/ames_tests.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
                zlib.artifact("z"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "boot-test",
            "pkg/vere/boot_tests.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "newt-test",
            "pkg/vere/newt_tests.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "vere-noun-test",
            "pkg/vere/noun_tests.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "unix-test",
            "pkg/vere/unix_tests.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            urbit_flags.items,
        );
        addTest(
            b,
            target,
            optimize,
            "benchmarks",
            "pkg/vere/benchmarks.c",
            &.{
                pkg_vere.artifact("vere"),
                pkg_noun.artifact("noun"),
                pkg_ur.artifact("ur"),
                pkg_ent.artifact("ent"),
                pkg_c3.artifact("c3"),
                gmp.artifact("gmp"),
                libuv.artifact("libuv"),
                lmdb.artifact("lmdb"),
                natpmp.artifact("natpmp"),
            },
            urbit_flags.items,
        );
    }
}

fn addTest(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
    file: []const u8,
    deps: []const *std.Build.Step.Compile,
    cflags: []const []const u8,
) void {
    const test_step = b.step(name, b.fmt("Build & run: {s}", .{file}));

    const test_exe = b.addExecutable(.{
        .name = name,
        .target = target,
        .optimize = optimize,
    });

    if (target.result.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            test_exe.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            test_exe.addLibraryPath(macos_sdk.?.path("usr/lib"));
            test_exe.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    test_exe.stack_size = 0;

    test_exe.linkLibC();
    for (deps) |dep| {
        test_exe.linkLibrary(dep);
    }

    test_exe.addCSourceFiles(.{ .files = &.{file}, .flags = cflags });

    const run_unit_tests = b.addRunArtifact(test_exe);
    run_unit_tests.skip_foreign_checks = true;
    test_step.dependOn(&run_unit_tests.step);
}
