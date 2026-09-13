"""Project-local macro for a translated game's dump binary and sentinel test."""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_decomp//decomp:sentinel.bzl", "sentinel_test")
load("//bazel:copts.bzl", "CXX_COPTS")
load("//bazel:private_tool_output.bzl", "file_sha256", "resource_tool_output")

_PRIVATE_TEST_TAGS = [
    "manual",
    "no-remote",
    "no-remote-cache",
    "no-remote-exec",
    "requires-private-data",
]

def variant_diagnostic_pair(
        binary_name,
        test_name,
        src,
        resources,
        sentinel,
        deps,
        size = "medium"):
    """Defines one diagnostic CLI and its private-data digest test."""
    cc_binary(
        name = binary_name,
        srcs = [src],
        copts = CXX_COPTS,
        target_compatible_with = ["@platforms//os:windows"],
        visibility = ["//:__pkg__"],
        deps = deps,
    )

    output_name = test_name + "_output"
    digest_name = test_name + "_digest"
    resource_tool_output(
        name = output_name,
        resources = resources,
        tool = ":" + binary_name,
        tool_writes_output = True,
        tags = _PRIVATE_TEST_TAGS,
    )
    file_sha256(
        name = digest_name,
        src = ":" + output_name,
        tags = _PRIVATE_TEST_TAGS,
    )
    sentinel_test(
        name = test_name,
        actual = ":" + digest_name,
        expected = sentinel,
        size = size,
        tags = _PRIVATE_TEST_TAGS,
        visibility = ["//:__pkg__"],
    )
