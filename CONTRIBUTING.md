# Contributing

## Workflow

Before beginning any unit of work, you should ensure you have a GitHub issue
detailing the scope of the work. This could be an issue someone else filed and
has been assigned to you (or you've assigned to yourself) or a new issue you
filed specifically for this unit of work. As much as possible, discussion of the
work should take place in the issue. When this is not possible, please update
the issue with relevant details from any offline conversations. Each issue
should provide a clear and thorough history of the work from inception to
completion.

### Branch Names

Every branch that you intend to put up for review must adhere to the form
`issue/N/<...>`, where `N` is the number of the issue that the branch corresponds
to and `<...>` is an optional short description of the branch to aid in
readability. If `<...>` is omitted, the `/` should be omitted as well, which
makes `issue/N` a well-formed branch name.

### Commits

Commit messages should be written in an imperative style and include a mandatory
short description and optional long description.

```
Require a short description

Optionally add a long description.
```

### Pull Requests and Merges

When your work is ready for review, open a pull request, making sure to link
to the tracking issue in the description, which should be formatted as follows
(where `N` is the number of this work's tracking issue):

```
### Description

Resolves #N.

Thoroughly describe the changes made.

### Related

Reference any related issues, links, papers, etc. here.
```

Tests will run automatically via GitHub Actions when you open a pull request or
push new commits to an existing pull request.

Once you've collected and addressed feedback and are ready to merge, squash and
merge the pull request. Use the default squash commit message. Assuming that you
properly included the "Resolves #N." directive in the pull request description,
merging will automatically close the tracking issue associated with the pull
request.


## Development Environment

Although you likely have an identity on the live network, developing on the live
network is high-risk and largely unnecessary. Instead, standard practice is to
work on a fake ship. Fake ships use deterministic keys derived from the ship's
address, don't communicate on the live network, and can communicate with other
fake ships over the local loopback.

### Boot a New Fake Ship

To boot a new fake ship, pass the `-F` flag and a valid Urbit ship name to
`urbit`:

```console
$ bazel build :urbit
$ ln -s bazel-bin/pkg/vere/urbit urbit
$ ./urbit -F <ship>
```

By default, booting a fake ship will use the same pre-compiled kernelspace-- 
a "pill"-- that livenet ships use, which leads to a non-trivial boot time on the
order of tens of minutes. However, using a development specific pill-- a "solid"
pill-- the time to boot a new fake ship can be reduced to a few minutes.

The solid pill (and other pills) live in the [Urbit repo][urbit]. To boot using
the solid pill, download the pill and then run:

```console
$ ./urbit -F <ship> -B solid.pill
```

Instead of downloading the pill, you can also generate one yourself using
[`dojo`][dojo]:

```console
dojo> .urbit/pill +solid
```

This will write the pill to `<pier>/.urb/put/urbit.pill` (note that `<pier>` is
the ship directory), which you can then use to boot a new ship:

```console
$ ./urbit -F <another-ship> -B <pier>/.urb/put/urbit.pill
```

### Launch an Existing Fake Ship

To launch an existing fake ship, supply the pier (the ship directory), which is
simply the name of the ship[^1], to `urbit`:

```console
$ ./urbit <ship>
```


[^1]: Unless you specify the pier using the `-c` flag.
