# Building Vere

We use [`bazel`][bazel][^1] to build Vere, which is packaged as a single binary,
`urbit`. We support the following `(host, target)` pairs, where the host platform
is where [`bazel`][bazel] runs and the target platform is where `urbit` will
run:

----------------------------------
 Host Platform  | Target Platform
----------------------------------
 `linux-x86_64` | `linux-arm64`
 `linux-x86_64` | `linux-x86_64`
 `macos-arm64`  | `macos-arm64`
 `macos-x86_64` | `macos-x86_64`

## Prerequisites

### Linux

We use [musl libc][musl libc] instead [glibc][glibc] on Linux, which enables us
to generate statically linked binaries. As a prerequisite, you need to install
the [musl libc][musl libc] toolchain appropriate for your target platform.

#### x86_64

To install the `x86_64-linux-musl-gcc` toolchain at
`/usr/local/x86_64-linux-musl-gcc`, run:
```console
$ bazel run //bazel/toolchain:x86_64-linux-musl-gcc
```

This will take a few minutes.

#### aarch64

To install the `aarch64-linux-musl-gcc` toolchain at
`/usr/local/aarch64-linux-musl-gcc`, run:
```console
$ bazel run //bazel/toolchain:aarch64-linux-musl-gcc
```

This will take a few minutes.

### macOS

Ensure you have up-to-date versions of `bazelisk`, `automake`, and `libtool`:
```console
$ brew install bazelisk automake libtool
```

## Build Commands

Once you install the prerequisites, you're ready to build. If you're performing
a native build (i.e. one in which the host platform and target platform are the
same), run:
```console
$ bazel build //pkg/...
```

If you're performing a cross-platform build, you need to specify the target
platform in the build command:
```console
$ bazel build --platforms=//:<target-platform> //pkg/...
```

## Test Commands

You can build and run unit tests only on native builds. If you have a native
build and want to run all unit tests, run:
```console
$ bazel test //pkg/...
```

If you want to run a specific test, say
[`pkg/noun/hashtable_tests.c`](pkg/noun/hashtable_tests.c), run:
```console
$ bazel test //pkg/noun:hashtable_tests
```

## Common Issues

If `bazel build` or `bazel test` generates an `undeclared inclusion(s) in rule`
error on macOS, the version of `clang` expected by the build system likely
doesn't match the version of `clang` installed on your system. To address this,
pass `--clang_version="<version_string>"` to the failing command.

[^1]: If you're interested in digging into the details of the build system,
      check out [`WORKSPACE.bazel`](WORKSPACE.bazel),
      [`BUILD.bazel`](BUILD.bazel), [`bazel/`](bazel), and the multiple
      `BUILD.bazel` files in [`pkg/`](pkg).

[bazel]: https://bazel.build
[glibc]: https://www.gnu.org/software/libc
[musl libc]: https://musl.libc.org
