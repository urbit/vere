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

See [INSTALL.md](INSTALL.md).

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
