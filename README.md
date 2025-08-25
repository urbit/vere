# Urbit Runtime

[Urbit][urbit] is a personal server stack built from scratch. This repository
contains [Urbit's runtime environment][vere], the lowest layer of the Urbit
stack, which includes the Nock virtual machine, I/O drivers, event log, and
snapshotting system.

## Getting Started

If you've never used Urbit before, head over [here](get-on-urbit) to get on
Urbit.

For a high-level overview of the salient aspects of Urbit's architecture, visit
[docs.urbit.org][docs].

## Packages

Urbit's runtime is broken down into a few separate layers, each of which is
defined in its own package:

- [`pkg/c3`](pkg/c3): A set of basic utilities for writing Urbit's style of C.
- [`pkg/ent`](pkg/ent): A cross-platform wrapper for `getentropy(2)`.
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

[docs]: https://docs.urbit.org
[contributing]: https://github.com/urbit/urbit/blob/master/CONTRIBUTING.md
[cue]: https://docs.urbit.org/hoon/stdlib/2p#cue
[get-on-urbit]: https://docs.urbit.org/get-on-urbit
[jam]: https://docs.urbit.org/hoon/stdlib/2p#jam
[urbit]: https://urbit.org
[vere]: https://docs.urbit.org/build-on-urbit/runtime
