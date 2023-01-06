# Maintainers' Guide

## Branch organization

The essence of this branching scheme is that you create "release branches" of
independently releasable units of work.  These can then be released by their
maintainers when ready.

### Master branch

Master is what's released on the network.  Deployment instructions are in the
next section, but tagged releases should always come from this branch.

### Release branches

`next/vere` is the release branch.

All code must be reviewed before being pushed to the release branch.  Thus,
issue branches should be PR'd against a release branch, not master.

### Other cases

Outside contributors can generally target their PRs against master unless
specifically instructed.  Maintainers should retarget those branches as
appropriate.

If a commit is not something that goes into a release (eg changes to README or
CI), it may be committed straight to master.

If a hotfix is urgent, it may be PR'd straight to master.  This should only be
done if you reasonably expect that it will be released soon and before anything
else is released.

If a series of commits that you want to release is on a release branch, but you
really don't want to release the whole branch, you must cherry-pick them onto
another release branch.  Cherry-picking isn't ideal because those commits will
be duplicated in the history, but it won't have any serious side effects.


## Hotfixes

Here lies an informal guide for making hotfix releases and deploying them to
the network.

Ideally, hotfixes should consist of a single commit, targeting a problem that 
existed in the latest runtime at the time. 

### If the thing is acceptable to merge, merge it to master

Unless it's very trivial, it should probably have a single "credible looking"
review from somebody else on it.

### Tag the resulting commit

You can get the "contributions" section by the shortlog between the
last release and this release:

```
git shortlog LAST_RELEASE..
```

I originally tried to curate this list somewhat, but now just paste it
verbatim.  If it's too noisy, yell at your colleagues to improve their commit
messages.

Try to include a high-level summary of the changes in the "release notes"
section.  You should be able to do this by simply looking at the git log and
skimming the commit descriptions (or perhaps copying some of them in verbatim).
If the commit descriptions are too poor to easily do this, then again, yell at
your fellow contributors to make them better in the future.

Tag the release as `urbit-vx.y`.  The tag format should look something like
this:

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
