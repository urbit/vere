# Building Vere

We use [`bazel`][bazel][^1] to build Vere, which is packaged as a single binary,
`urbit`. We support the following `(host, target)` pairs, where the host platform
is where [`bazel`][bazel] runs and the target platform is where `urbit` will
run:

| Host Platform  | Target Platform |
|----------------|-----------------|
| `linux-aarch64`| `linux-aarch64` |
| `linux-x86_64` | `linux-x86_64`  |
| `macos-aarch64`| `macos-aarch64` |
| `macos-x86_64` | `macos-x86_64`  |

## Prerequisites

The first step is to install `bazel` using the instructions found on the [`bazel`][bazel-install][^1] site.

All platforms require the `automake` and `libtool` suite of tools installed in
order to build Vere. Before going any further, install them using your package
manager. For example, on macOS:

```console
brew install automake libtool
```

### Linux

After installing `automake` and `libtool`, you need to install the [musl libc] toolchain.  We use [musl libc][musl libc] instead of [glibc][glibc] on Linux, which enables us to generate statically linked binaries. As a prerequisite, you need to install the [musl libc][musl libc] toolchain appropriate for your target platform.

#### x86_64

To install the `x86_64-linux-musl-gcc` toolchain at
`/usr/local/x86_64-linux-musl-gcc`, run:
```console
bazel run //bazel/toolchain:x86_64-linux-musl-gcc
```

This will take a few minutes.

#### aarch64

To install the `aarch64-linux-musl-gcc` toolchain at
`/usr/local/aarch64-linux-musl-gcc`, run:
```console
bazel run //bazel/toolchain:aarch64-linux-musl-gcc
```

This will take a few minutes.

### macOS

After installing `automake` and `libtool`, you're ready to build Vere.

## Build Commands

Once you install the prerequisites, you're ready to build:
```console
bazel build :urbit
```

If you want a debug build, which changes the optimization level from `-O3` to
`-O0` and includes more debugging information, specify the `dbg` configuration:
```console
bazel build --config=dbg :urbit
```
Note that you cannot change the optimization level for third party
dependencies--those targets specified in `bazel/third_party`--from the command
line.

You can turn on CPU and memory debugging by defining `U3_CPU_DEBUG` and
`U3_MEMORY_DEBUG`, respectively:
```console
bazel build --copt='-DU3_CPU_DEBUG' --copt='-DU3_MEMORY_DEBUG' :urbit
```
Note that defining these two debug symbols will produce ships that are
incompatible with binaries without these two debug symbols defined.

If you need to specify arbitrary C compiler or linker options, use
[`--copt`][copt] or [`--linkopt`][linkopt], respectively:
```console
bazel build --copt='-O0' :urbit
```

Note [`--copt`][copt] can be used to specify any C compiler options, not just
optimization levels.

## Test Commands

You can build and run unit tests only on native builds. If you have a native
build and want to run all unit tests, run:
```console
bazel test --build_tests_only ...
```

If you want to run a specific test, say
[`pkg/noun/hashtable_tests.c`](pkg/noun/hashtable_tests.c), run:
```console
bazel test //pkg/noun:hashtable_tests
```

## Build Configuration File

Any options you specify at the command line can instead be specified in
`.user.bazelrc` if you find yourself using the same options over and over. This
file is not tracked by `git`, so whatever you add to it will not affect anyone
else. As an example, if you want to change the optimization level but don't want
type `--copt='-O0'` each time, you can do the following:
```console
echo "build --copt='-O0'" >> .user.bazelrc
bazel build :urbit
```

For more information on Bazel configuration files, consult the
[Bazel docs][bazel-config].

## Common Issues

If `bazel build` or `bazel test` generates an `undeclared inclusion(s) in rule`
error on macOS, the version of `clang` expected by the build system likely
doesn't match the version of `clang` installed on your system. To address this,
run `clang --version` and pass the version number via
`--clang_version="<version_string>"` to the failing command.

[^1]: If you're interested in digging into the details of the build system,
      check out [`WORKSPACE.bazel`](WORKSPACE.bazel),
      [`BUILD.bazel`](BUILD.bazel), [`bazel/`](bazel), and the multiple
      `BUILD.bazel` files in [`pkg/`](pkg).

[bazel]: https://bazel.build
[bazel-config]: https://bazel.build/run/bazelrc
[bazel-install]: https://bazel.build/install
[copt]: https://bazel.build/docs/user-manual#copt
[glibc]: https://www.gnu.org/software/libc
[linkopt]: https://bazel.build/docs/user-manual#linkopt
[musl libc]: https://musl.libc.org
