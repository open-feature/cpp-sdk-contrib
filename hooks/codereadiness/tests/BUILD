load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "semver_test",
    size = "small",
    srcs = ["semver_test.cpp"],
    deps = [
        "//hooks/codereadiness/src:semver",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "semver_comparator_test",
    size = "small",
    srcs = ["semver_comparator_test.cpp"],
    deps = [
        "//hooks/codereadiness/src:semver_comparator",
        "//hooks/codereadiness/src:version_comparator",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "codereadiness_test",
    size = "small",
    srcs = ["codereadiness_test.cpp"],
    deps = [
        "//hooks/codereadiness/src:codereadiness",
        "//hooks/codereadiness/src:version_comparator",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest_main",
        "@openfeature_cpp_sdk//openfeature:client",
        "@openfeature_cpp_sdk//openfeature:error_code",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
        "@openfeature_cpp_sdk//openfeature:evaluation_options",
        "@openfeature_cpp_sdk//openfeature:flag_evaluation_details",
        "@openfeature_cpp_sdk//openfeature:flag_metadata",
        "@openfeature_cpp_sdk//openfeature:openfeature_api",
        "@openfeature_cpp_sdk//openfeature:reason",
        "@openfeature_cpp_sdk//openfeature/memory_provider:flag",
        "@openfeature_cpp_sdk//openfeature/memory_provider:in_memory_provider",
    ],
)
