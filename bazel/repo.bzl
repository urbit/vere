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
