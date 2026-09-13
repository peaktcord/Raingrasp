"""Compiler options shared by the repository's C++ packages."""

MSVC_COPTS = [
    "/std:c++17",
    "/EHsc",
    "/utf-8",
]

POSIX_COPTS = [
    "-std=c++17",
    "-fexceptions",
]

CXX_COPTS = select({
    "@rules_cc//cc/compiler:msvc-cl": MSVC_COPTS,
    "//conditions:default": POSIX_COPTS,
})
