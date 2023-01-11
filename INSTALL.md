# Building Vere

We use [`bazel`][bazel][^1] to build Vere, which is packaged as a single binary,
`urbit`. We support the following `(host, target)` pairs, where the host platform
is where [`bazel`][bazel] runs and the target platform is where `urbit` will
run:

----------------------------------
 Host Platform  | Target Platform
----------------------------------
 `linux-aarch64 | `linux-aarch64`
 `linux-x86_64` | `linux-x86_64`
 `macos-aarch64`| `macos-aarch64`
 `macos-x86_64` | `macos-x86_64`

## Prerequisites

All platforms require the `automake` and `libtool` suite of tools installed in
order to build Vere. Before going any further, install them using your package
manager. For example, on macOS:

```console
$ brew install automake libtool
```

### Linux

We use [musl libc][musl libc] instead of [glibc][glibc] on Linux, which enables us
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

## Build Commands

Once you install the prerequisites, you're ready to build:
```console
$ bazel build :urbit
```

The default optimization level is `-O3`, but if you want to specify a different
optimization level, use [`--copt`][copt]:
```console
$ bazel build --copt='-O0' :urbit
```

Note [`--copt`][copt] can be used to specify any C compiler options, not just
optimization levels.

## Test Commands

You can build and run unit tests only on native builds. If you have a native
build and want to run all unit tests, run:
```console
$ bazel test --build_tests_only ...
```

If you want to run a specific test, say
[`pkg/noun/hashtable_tests.c`](pkg/noun/hashtable_tests.c), run:
```console
$ bazel test //pkg/noun:hashtable_tests
```

## Build Configuration File

Any options you specify at the command line can instead be specified in
`.user.bazelrc` if you find yourself using the same options over and over. This
file is not tracked by `git`, so whatever you add to it will not affect anyone
else. As an example, if you want to change the optimization level but don't want
type `--copt='-O0'` each time, you can do the following:
```console
$ echo "build --copt='-O0'" >> .user.bazelrc
$ bazel build :urbit
```

For more information on Bazel configuration files, consult the
[Bazel docs][bazel-config].

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
[bazel-config]: https://bazel.build/run/bazelrc
[copt]: https://bazel.build/docs/user-manual#copt
[glibc]: https://www.gnu.org/software/libc
[musl libc]: https://musl.libc.org
