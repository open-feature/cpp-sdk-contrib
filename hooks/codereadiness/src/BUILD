load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Interface for comparing the binary's current version against a flag's
# required minimum version.
cc_library(
    name = "version_comparator",
    hdrs = ["version_comparator.h"],
    include_prefix = "codereadiness",
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

# SemVer 2.0.0 parsing and precedence.
cc_library(
    name = "semver",
    srcs = ["semver.cpp"],
    hdrs = ["semver.h"],
    include_prefix = "codereadiness",
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# Default VersionComparator implementation, based on SemVer.
cc_library(
    name = "semver_comparator",
    srcs = ["semver_comparator.cpp"],
    hdrs = ["semver_comparator.h"],
    include_prefix = "codereadiness",
    deps = [
        ":semver",
        ":version_comparator",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# OpenFeature `after` hook that fails flag evaluation when the current code
# version is lower than the flag's required minimum version.
cc_library(
    name = "codereadiness",
    srcs = ["codereadiness.cpp"],
    hdrs = ["codereadiness.h"],
    include_prefix = "codereadiness",
    deps = [
        ":semver_comparator",
        ":version_comparator",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@openfeature_cpp_sdk//openfeature:flag_metadata",
        "@openfeature_cpp_sdk//openfeature:general_flag_evaluation_details",
        "@openfeature_cpp_sdk//openfeature:general_hook",
        "@openfeature_cpp_sdk//openfeature:general_hook_context",
        "@openfeature_cpp_sdk//openfeature:hook_hints",
    ],
)
