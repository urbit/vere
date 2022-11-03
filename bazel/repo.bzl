load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Adds a `version` attribute to `http_archive`.
def versioned_http_archive(name, version, **kwargs):
    if kwargs.get("url") != None:
        kwargs["url"] = kwargs["url"].format(version = version)

    if kwargs.get("urls") != None:
        for index, url in enumerate(kwargs["urls"]):
            kwargs["urls"][index] = url.format(version = version)

    if kwargs.get("patches") != None:
        for index, patch in enumerate(kwargs["patches"]):
            kwargs["patches"][index] = patch.format(version = version)

    if kwargs.get("strip_prefix") != None:
        kwargs["strip_prefix"] = kwargs["strip_prefix"].format(version = version)

    maybe(http_archive, name, **kwargs)

# Adds a `version` attribute to `http_file`.
def versioned_http_file(name, version, **kwargs):
    if kwargs.get("url") != None:
        kwargs["url"] = kwargs["url"].format(version = version)

    if kwargs.get("urls") != None:
        for index, url in enumerate(kwargs["urls"]):
            kwargs["urls"][index] = url.format(version = version)

    if kwargs.get("strip_prefix") != None:
        kwargs["strip_prefix"] = kwargs["strip_prefix"].format(version = version)

    maybe(http_file, name, **kwargs)

def add_http_archives(base_name, version, **kwargs):
    """Adds versioned `http_archive` rules for each platform that patches specified.

    Args:
      base_name: The base name of the archives, to be suffixed with the platform.
      version: The version of the archives.
      **kwargs: The keyword arguments to pass to `http_archive`.
    """

    # Add the archives for each platform.
    # TODO: Maintain canonical list of platforms elsewhere?
    platforms = [
        "linux-x86_64",
        "linux-arm64",
        "macos-x86_64",
        "macos-arm64",
        "windows-x86_64",
        "openbsd-x86_64",
    ]
    for platform in platforms:
        kwargs_copy = dict(kwargs)
        patches = kwargs.get("patches", {}).get(platform)
        kwargs_copy["patches"] = patches
        kwargs_copy["patch_args"] = ["-p1"] if patches != None else None
        versioned_http_archive(
            name = base_name + "_" + platform,
            version = version,
            **kwargs_copy
        )
