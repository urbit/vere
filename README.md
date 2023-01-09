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

We use [`bazel`][bazel] to build Urbit's runtime. We support the following
platforms:

- `darwin-arm64` (macOS running Apple Silicon)
- `darwin-x86_64` (macOS running Intel silicon)
- `linux-arm64`
- `linux-x86_64`
- `openbsd-x86_64`
- `mingw-x86_64`

To build the `urbit` binary, the primary artifact of this repository, ensure
that you're on a supported platform and that you have an up-to-date version of
[`bazel`][bazel], and then run:

```console
$ bazel build :urbit
```

The build will take a while since `bazel` has to download and build from source
all of `urbit`'s third-party dependencies.

To run the just-built `urbit` binary, run:

```console
$ bazel-bin/pkg/vere/urbit ...
```

Or, to save yourself a few keystrokes, create a symlink to the `urbit` binary in
the root of the repository:

```console
$ ln -s bazel-bin/pkg/vere/urbit urbit
$ ./urbit ...
```

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
