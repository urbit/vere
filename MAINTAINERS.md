# Maintainers' Guide

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

Ensure the release is marked as the 'latest' release and upload the four 
`.tgz` files to the release as `darwin.tgz` and `linux64.tgz`;
this allows us to programmatically retrieve the latest releases at
the corresponding platform's URL: `https://urbit.org/install/{platform}/latest`.

### Announce the update

Post an announcement to urbit-dev.  The tag annotation, basically, is fine here
-- I usually add the release binary URLs.  Check the urbit-dev archives for examples 
of these announcements.

Post the same announcement to the group feed of Urbit Community.
