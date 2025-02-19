const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "softblas",
        .target = target,
        .optimize = optimize,
    });

    const dep_c = b.dependency("softblas", .{
        .target = target,
        .optimize = optimize,
    });

    const softfloat = b.dependency("softfloat", .{
        .target = target,
        .optimize = optimize,
    });

    lib.addIncludePath(dep_c.path("include"));

    lib.addCSourceFiles(.{
        .root = dep_c.path(""),
        .files = &.{
            "src/softblas_state.c",
            "src/blas/level1/sasum.c",
            "src/blas/level1/dasum.c",
            "src/blas/level1/hasum.c",
            "src/blas/level1/qasum.c",
            "src/blas/level1/saxpy.c",
            "src/blas/level1/daxpy.c",
            "src/blas/level1/haxpy.c",
            "src/blas/level1/qaxpy.c",
            "src/blas/level1/scopy.c",
            "src/blas/level1/dcopy.c",
            "src/blas/level1/hcopy.c",
            "src/blas/level1/qcopy.c",
            "src/blas/level1/sdot.c",
            "src/blas/level1/ddot.c",
            "src/blas/level1/hdot.c",
            "src/blas/level1/qdot.c",
            "src/blas/level1/snrm2.c",
            "src/blas/level1/dnrm2.c",
            "src/blas/level1/hnrm2.c",
            "src/blas/level1/qnrm2.c",
            "src/blas/level1/sscal.c",
            "src/blas/level1/dscal.c",
            "src/blas/level1/hscal.c",
            "src/blas/level1/qscal.c",
            "src/blas/level1/sswap.c",
            "src/blas/level1/dswap.c",
            "src/blas/level1/hswap.c",
            "src/blas/level1/qswap.c",
            "src/blas/level1/isamax.c",
            "src/blas/level1/idamax.c",
            "src/blas/level1/ihamax.c",
            "src/blas/level1/iqamax.c",
            "src/blas/level2/sgemv.c",
            "src/blas/level2/dgemv.c",
            "src/blas/level2/hgemv.c",
            "src/blas/level2/qgemv.c",
            "src/blas/level3/sgemm.c",
            "src/blas/level3/dgemm.c",
            "src/blas/level3/hgemm.c",
            "src/blas/level3/qgemm.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
        },
    });

    lib.installHeader(dep_c.path("include/softblas.h"), "softblas.h");

    lib.linkLibC();
    lib.linkLibrary(softfloat.artifact("softfloat"));
    b.installArtifact(lib);
}
