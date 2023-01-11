# Maintaining

## Overview

We use a three-stage release pipeline. Each stage of the release pipeline has
its own dedicated branch and a corresponding "train" (also called a pace).
Features and bug fixes progress through each stage--and are subject to testing
along the way--until they're eventually released to the live network. This
pipeline automates our release process, making it much easier to quickly and
reliably ship code. It's also simple to reason about.

## Branches and Trains

The branches and their corresponding trains that comprise the stages of the
release pipeline are:

-----------------------------------------
 Branch    | Train  | Target audience
-----------------------------------------
 `develop` | `edge` | Runtime developers
 `release` | `soon` | Early adopters
 `master`  | `live` | Everyone else

`develop` is the default branch in the repo, which means that all new pull
requests target it by default. The general flow of a new feature or bug fix
through the pipeline is:

```console
feature branch ----> develop ----> release ----> master
                        |             |             |
                    deployed to    deployed to   deployed to
                    edge train     soon train    live train
```

If an issue arises in the course of testing the `release` branch (because more
people are using `soon` than `edge`), a PR can be opened to target `release`.
If that's the case, the `master` needs to be merged back into `develop` after
`release` merges into `master` to ensure that `develop` gets the fix.

## Version Numbers

Each time a commit is pushed to `develop`, `release`, or `master`--say, when a
PR merges--we build and deploy a new version of the binary available for
consumption by anyone subscribed to that train (via `<pier>/.bin/pace`).

For `edge` and `soon`, each binary is given a version of the form
`{version number}-{shortened commit SHA}`, where `{version number}` is the
version number listed in the [version file in the root of this repo](./VERSION)
and `{shortened commit SHA}` is the shortened commit SHA of the commit the
binary was built from. This allows subscribers to `edge` and `soon` to
continually pull down new binaries via the `next` subcommand even when the
version number in the [version file](./VERSION) remains the same.

For `master`, each binary is given a version of the form `{version number}`,
where `{version number}` is simply the version number listed in the
[version file in the root of this repo](./VERSION).

Each time a release is cut (i.e. `develop` is merged into `release` to kick off
a release), the version number should be bumped on `develop` in anticipation of
the next release.

## Deploy Endpoints

Binaries are deployed to the following endpoints, where `{VN}` is the version
number in VERSION, `{CS}` is the shortened commit SHA of the commit the binary
is built off, and `{P}` is one of `linux-aarch64`, `linux-x86_64`,
`macos-aarch64`, and `macos-x86_64`:

- https://bootstrap.urbit.org/vere/edge/v{VN}-{CS}/vere-v{VN}-{P}
- https://bootstrap.urbit.org/vere/soon/v{VN}-{CS}/vere-v{VN}-{P}
- https://bootstrap.urbit.org/vere/live/v{VN}/vere-v{VN}-{P}

The most recently deployed version of a given train (pace) is uploaded to
https://bootstrap.urbit.org/vere/{T}/last, where `{T}` is one of `edge`, `soon`,
and `live`:

## Announcing a Release

A new version is deployed to the live net when new commits land on `master`. At
this point, we create a GitHub release to automatically tag the latest commit on
`master` as well as document the changes that went into the release. Ensure the
GitHub release is marked as "latest".  We also post an announcement to
urbit-dev and an identical one to the group feed of Urbit Community. Check the
urbit-dev archives for examples of these announcements

```
urbit-vx.y

Note that this release will by default boot fresh ships using an Urbit OS
va.b.c pill.

Release binaries:

(linux-aarch64)
https://bootstrap.urbit.org/urbit-vx.y-linux-aarch64.tgz

(linux-x86_64)
https://bootstrap.urbit.org/urbit-vx.y-linux-x86_64.tgz

(macos-aarch64)
https://bootstrap.urbit.org/urbit-vx.y-macos-aarch64.tgz

(macos-x86_64)
https://bootstrap.urbit.org/urbit-vx.y-macos-x86_64.tgz

Release notes:

  [..]

Contributions:

  [..]
```

TODO:
- Do we *need* to upload tarballs of the released binaries to the GitHub
  release?
