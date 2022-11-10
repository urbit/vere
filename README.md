# Urbit Runtime

[Urbit][urbit] is a personal server stack built from scratch. This repository
contains [Urbit's runtime environment][vere], the lowest layer of the Urbit
stack, which includes the Nock virtual machine, I/O drivers, event log, and
snapshotting system.


## Getting Started

For basic Urbit usage instructions, head over to [urbit.org][getting-started].
For a high-level overview of the salient aspects of Urbit's architecture, visit
[developers.urbit.org][technical-reference]. You might also be interested in
joining the [urbit-dev][mailing-list] mailing list.


## Packages

Urbit's runtime is broken down into a few separate layers, each of which is
defined in its own package:

- [`pkg/c3`](pkg/c3): A set of basic utilities for writing Urbit's style of C.
- [`pkg/ent`](pkg/ent): A cross-platform wrapper for `getentropy(2)`.
- [`pkg/urcrypt`](pkg/urcrypt): A standardized interface for calling various
  cryptographic functions used in the Urbit runtime.
- [`pkg/ur`](pkg/ur): An implementation of [jam][jam] and [cue][cue], Urbit's
  bitwise noun serialization and deserialization algorithms, respectively.
- [`pkg/noun`](pkg/noun): The Nock virtual machine and snapshotting system.
- [`pkg/vere`](pkg/vere): The I/O drivers, event log, and main event loop.


## Build

We use [`bazel`][bazel] to build Urbit's runtime, which is packaged as a single
binary, `urbit`. We support the following `(host, target)` pairs, where the host
platform is where [`bazel`][bazel] runs and the target platform is where `urbit`
will run:

--------------------------------------------------------------------------------
 Host Platform                        | Target Platform | Required Toolchain
--------------------------------------------------------------------------------
 `aarch64_linux_gnu_gcc-linux-x86_64` | `linux-arm64`   | `aarch64-linux_gnu_gcc`
 `gcc-linux-x86_64`                   | `linux-x86_64`  | `gcc`
 `clang-linux-x86_64`                 | `linux-x86_64`  | `clang`

Once you've identified your `(host, target)` pair, determine the version of the
pair's required toolchain and ensure you have an up-to-date version of
[`bazel`][bazel]. Then, run:

```console
$ bazel build --<toolchain>_version="<toolchain_version>" \
              --host_platform=//:<host_platform>          \
              --platforms=//:<target_platform> :urbit
```

For example, to build a `linux-x86_64` `urbit` binary on a `linux-x86_64`
machine using version `14.0.6` of the `clang` toolchain, run:

```console
$ bazel build --clang_version="14.0.6"            \
              --host_platform=//:gcc-linux-x86_64 \
              --platforms=//:linux-x86_64
```

And to build a `linux-arm64` `urbit` binary on a `linux-x86_64` machine using
version `12.2.0` of the `aarch64-linux-gnu-gcc` toolchain (which you'll have to
install), run:

```console
$ bazel build --aarch64_linux_gnu_gcc_version="12.2.0"              \
              --host_platform=//:aarch64_linux_gnu_gcc-linux-x86_64 \
              --platforms=//:linux-arm64
```

Specifying `--<toolchain>_version`, `--host_platform`, and `--platforms` for
each build is tedious and can be avoided by writing to `.user.bazelrc`:

```console
$ echo 'build --aarch64_linux_gnu_gcc_version="12.2.0"' >> .user.bazelrc
$ echo 'build --clang_version="14.0.6"'                 >> .user.bazelrc
$ echo 'build --gcc_version="12.2.0"'                   >> .user.bazelrc
$ echo 'build --host_platform=//:<host_platform>'       >> .user.bazelrc
$ echo 'build --platforms=//:<target_platform>'         >> .user.bazelrc
$ bazel build :urbit
```

To run the just-built `urbit` binary, run:

```console
$ bazel-bin/pkg/vere/urbit <snip>
```

Or, to save yourself a few keystrokes, create a symlink to the `urbit` binary in
the root of the repository:

```console
$ ln -s bazel-bin/pkg/vere/urbit urbit
$ ./urbit <snip>
```

The remaining commands in this section assume that `.user.bazlerc` specifies
`--host_platform` and `--platforms`. If not, `--host_platform` and `--platforms`
must be provided at the command line as in the build commands above.

To run all runtime tests, run:

```console
$ bazel test //...
```

or, to run a specific test, say
[`pkg/noun/hashtable_tests.c`](pkg/noun/hashtable_tests.c), run:

```console
$ bazel test //pkg/noun:hashtable_tests
```

If you're interested in digging into the details of the build system, check out
[`WORKSPACE.bazel`](WORKSPACE.bazel), [`BUILD.bazel`](BUILD.bazel),
[`bazel/`](bazel), and the multiple `BUILD.bazel` files in [`pkg/`](pkg).


## Contributing

Contributions of any form are more than welcome. Please take a look at our
[contributing guidelines][contributing] for details on our git practices, coding
styles, how we manage issues, and so on.


[bazel]: https://bazel.build
[contributing]: https://github.com/urbit/urbit/blob/master/CONTRIBUTING.md
[cue]: https://developers.urbit.org/reference/hoon/stdlib/2p#cue
[getting-started]: https://urbit.org/getting-started
[jam]: https://developers.urbit.org/reference/hoon/stdlib/2p#jam
[mailing-list]: https://groups.google.com/a/urbit.org/forum/#!forum/dev
[urbit]: https://urbit.org
[vere]: https://developers.urbit.org/reference/glossary/vere
[technical-reference]: https://developers.urbit.org/reference
